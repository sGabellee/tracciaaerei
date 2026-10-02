#include "flight_selectors.h"
#include "config.h"

void FlightSelectors::fillFromState(TrackedFlight& tf, const AircraftState& ac) {
    tf.valid = true;
    tf.icao24 = ac.icao24;
    tf.callsign = ac.callsign;
    tf.lastKnownPos = GeoPoint{ ac.lat, ac.lon };
    tf.velocityMs = ac.velocityMs;
    tf.trueTrackDeg = ac.trueTrackDeg;
    tf.baroAltitudeM = ac.baroAltitudeM;
    tf.lastUpdateMs = millis();
    // tf.route NON viene toccata qui: se tf è un oggetto già tracciato, la
    // rotta già ottenuta va preservata (evita richieste AeroDataBox ripetute).
}

bool FlightSelectors::isInboundToCaselle(const AircraftState& ac) const {
    if (isnan(ac.trueTrackDeg) || isnan(ac.baroAltitudeM)) return false;
    if (ac.baroAltitudeM > CASELLE_MAX_ALTITUDE_M) return false;

    double diff = fmod(fabs(ac.trueTrackDeg - RUNWAY36_HEADING_DEG), 360.0);
    if (diff > 180.0) diff = 360.0 - diff;
    return diff <= RUNWAY_HEADING_TOLERANCE_DEG;
}

bool FlightSelectors::lookLikeLanded(const TrackedFlight& tf, double altThresholdM, double proximityKm) const {
    if (!tf.valid || isnan(tf.baroAltitudeM)) return false;
    if (tf.baroAltitudeM > altThresholdM) return false;
    double d = geoDistanceMeters(tf.lastKnownPos, GeoPoint{ CASELLE_AIRPORT_LAT, CASELLE_AIRPORT_LON });
    return d <= proximityKm * 1000.0;
}

void FlightSelectors::ensureRoute(TrackedFlight& tf) {
    if (tf.route.valid || tf.callsign.isEmpty()) return;

    for (auto& entry : routeCache_) {
        if (entry.used && entry.callsign == tf.callsign) {
            tf.route = entry.route;
            return;
        }
    }

    RouteInfo fetched;
    if (aeroDataBoxFetchRoute(tf.callsign, fetched)) {
        tf.route = fetched;
        RouteCacheEntry& slot = routeCache_[routeCacheNext_];
        slot.callsign = tf.callsign;
        slot.route = fetched;
        slot.used = true;
        routeCacheNext_ = (routeCacheNext_ + 1) % ROUTE_CACHE_SIZE;
    }
}

void FlightSelectors::updateCaselle(const std::vector<AircraftState>& states) {
    CaselleView& v = caselleView_;

    if (v.state == CaselleState::IDLE) {
        const AircraftState* best = nullptr;
        double bestDist = 1e18;
        for (const auto& ac : states) {
            if (!isInboundToCaselle(ac)) continue;
            double d = geoDistanceMeters(GeoPoint{ CASELLE_TRIGGER_LAT, CASELLE_TRIGGER_LON }, GeoPoint{ ac.lat, ac.lon });
            if (d <= CASELLE_TRIGGER_RADIUS_KM * 1000.0 && d < bestDist) {
                bestDist = d;
                best = &ac;
            }
        }
        if (best) {
            v.current = TrackedFlight{};
            fillFromState(v.current, *best);
            ensureRoute(v.current);
            v.state = CaselleState::TRACKING;
        }
    } else if (v.state == CaselleState::TRACKING) {
        const AircraftState* found = nullptr;
        for (const auto& ac : states) {
            if (ac.icao24 == v.current.icao24) { found = &ac; break; }
        }

        if (found) {
            fillFromState(v.current, *found);
            if (lookLikeLanded(v.current, LANDED_ALTITUDE_M, LANDED_PROXIMITY_KM)) {
                v.state = CaselleState::LANDED;
                v.landedAnimationStartMs = millis();
            }
        } else if (lookLikeLanded(v.current, LANDED_FALLBACK_ALTITUDE_M, LANDED_FALLBACK_PROXIMITY_KM)) {
            // Sparito dal poll (tipico vicino al suolo): atterrato solo se
            // l'ultimo dato noto era già in corto finale, non per un buco dati qualsiasi.
            v.state = CaselleState::LANDED;
            v.landedAnimationStartMs = millis();
        }
        // altrimenti: teniamo l'ultimo dato noto a schermo, riproveremo al prossimo poll
    }

    // Secondo velivolo in coda: cercato sia durante TRACKING che durante
    // l'animazione LANDED, finché non ne abbiamo già uno in attesa.
    if (!v.hasQueued && v.state != CaselleState::IDLE) {
        const AircraftState* best = nullptr;
        double bestDist = 1e18;
        for (const auto& ac : states) {
            if (ac.icao24 == v.current.icao24) continue;
            if (!isInboundToCaselle(ac)) continue;
            double d = geoDistanceMeters(GeoPoint{ CASELLE_TRIGGER_LAT, CASELLE_TRIGGER_LON }, GeoPoint{ ac.lat, ac.lon });
            if (d <= CASELLE_TRIGGER_RADIUS_KM * 1000.0 && d < bestDist) {
                bestDist = d;
                best = &ac;
            }
        }
        if (best) {
            v.queued = TrackedFlight{};
            fillFromState(v.queued, *best);
            ensureRoute(v.queued);
            v.hasQueued = true;
        }
    }
}

void FlightSelectors::updateNearest(const std::vector<AircraftState>& states) {
    const AircraftState* best = nullptr;
    double bestDist = 1e18;
    for (const auto& ac : states) {
        double d = geoDistanceMeters(GeoPoint{ HOME_LAT, HOME_LON }, GeoPoint{ ac.lat, ac.lon });
        if (d <= NEAREST_RADIUS_KM * 1000.0 && d < bestDist) {
            bestDist = d;
            best = &ac;
        }
    }

    if (best) {
        TrackedFlight tf;
        fillFromState(tf, *best);
        ensureRoute(tf);
        nearestView_.current = tf;
    } else {
        nearestView_.current = TrackedFlight{};
    }
}

void FlightSelectors::onPoll(const std::vector<AircraftState>& states) {
    updateCaselle(states);
    updateNearest(states);
}

void FlightSelectors::onTick() {
    CaselleView& v = caselleView_;
    if (v.state != CaselleState::LANDED) return;

    if (millis() - v.landedAnimationStartMs >= LANDED_ANIMATION_MS) {
        if (v.hasQueued) {
            v.current = v.queued;
            v.hasQueued = false;
            v.state = CaselleState::TRACKING;
        } else {
            v.current = TrackedFlight{};
            v.state = CaselleState::IDLE;
        }
    }
}

#pragma once

#include "aerodatabox.h"
#include "config.h"
#include "geo.h"
#include "opensky.h"
#include <Arduino.h>
#include <vector>

enum class CaselleState { IDLE, TRACKING, LANDED };

struct TrackedFlight {
    bool valid = false;
    String icao24;
    String callsign;
    GeoPoint lastKnownPos{};
    double velocityMs = NAN;
    double trueTrackDeg = NAN;
    double baroAltitudeM = NAN;
    unsigned long lastUpdateMs = 0;
    RouteInfo route;

    // Posizione stimata "adesso", estrapolata da ultima posizione nota +
    // velocità/rotta. Usata per la freccia direzionale nella vista Più vicino.
    GeoPoint estimatedPosition(unsigned long nowMs, double maxExtrapolationSec = 60.0) const {
        if (isnan(velocityMs) || isnan(trueTrackDeg) || lastUpdateMs == 0) {
            return lastKnownPos;
        }
        double dt = (nowMs - lastUpdateMs) / 1000.0;
        if (dt < 0 || dt > maxExtrapolationSec) dt = 0;  // oltre il limite senza un poll, non estrapoliamo
        return geoExtrapolate(lastKnownPos, velocityMs, trueTrackDeg, dt);
    }
};

struct CaselleView {
    CaselleState state = CaselleState::IDLE;
    TrackedFlight current;
    bool hasQueued = false;
    TrackedFlight queued;
    unsigned long landedAnimationStartMs = 0;
};

struct NearestView {
    TrackedFlight current;  // current.valid == false se nulla in raggio
};

class FlightSelectors {
public:
    // Da chiamare ad ogni poll OpenSky riuscito (~20-25s).
    void onPoll(const std::vector<AircraftState>& states);

    // Da chiamare periodicamente (es. ogni ~1s) per far avanzare l'animazione
    // di atterraggio e le eventuali transizioni di coda.
    void onTick();

    // Abilita/disabilita le richieste AeroDataBox (bloccanti per qualche
    // secondo): la vista mappa non mostra la rotta e non deve fermarsi. Alla
    // riabilitazione recupera le rotte rimaste mancanti.
    void setRouteLookupEnabled(bool enabled);

    const CaselleView& caselle() const { return caselleView_; }
    const NearestView& nearest() const { return nearestView_; }

private:
    CaselleView caselleView_;
    NearestView nearestView_;

    struct RouteCacheEntry {
        String callsign;
        RouteInfo route;
        bool used = false;
    };
    RouteCacheEntry routeCache_[ROUTE_CACHE_SIZE];
    int routeCacheNext_ = 0;
    bool routeLookupEnabled_ = true;

    void updateCaselle(const std::vector<AircraftState>& states);
    void updateNearest(const std::vector<AircraftState>& states);
    void ensureRoute(TrackedFlight& tf);
    bool isInboundToCaselle(const AircraftState& ac) const;
    bool lookLikeLanded(const TrackedFlight& tf, double altThresholdM, double proximityKm) const;
    bool reachedAirport(const TrackedFlight& tf, unsigned long nowMs) const;
    static void fillFromState(TrackedFlight& tf, const AircraftState& ac);
};

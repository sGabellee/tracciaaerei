#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "config.h"

// Pannello GC9A01 rotondo 240x240, modulo SPI a 7 pin (RST,CS,DC,SDA,SCL,GND,VCC)
// - niente pin backlight separato, quindi niente controllo PWM luminosità qui.
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_GC9A01 _panel_instance;
    lgfx::Bus_SPI _bus_instance;

public:
    LGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;
            cfg.freq_read = 16000000;
            cfg.spi_3wire = true;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = PIN_DISPLAY_SCLK;
            cfg.pin_mosi = PIN_DISPLAY_MOSI;
            cfg.pin_miso = -1;
            cfg.pin_dc = PIN_DISPLAY_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = PIN_DISPLAY_CS;
            cfg.pin_rst = PIN_DISPLAY_RST;
            cfg.pin_busy = -1;
            cfg.panel_width = 240;
            cfg.panel_height = 240;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.readable = false;
            // Molti moduli GC9A01 mostrano i colori invertiti finché non si
            // imposta questo flag: se all'accensione il nero è bianco e
            // viceversa, prova a metterlo a false.
            cfg.invert = true;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            _panel_instance.config(cfg);
        }
        setPanel(&_panel_instance);
    }
};

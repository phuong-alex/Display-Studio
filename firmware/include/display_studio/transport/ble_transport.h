#pragma once

namespace DisplayStudio::Transport {
class BleTransport {
public:
    void begin();
    void loop();
    bool connected() const;
};

BleTransport& bleTransport();
}

/* Buffer ownership stays here, including discarded data while suspending. */
#include "s31_uhci.h"
bool ink_s31_cell_rx(void (*feed)(const uint8_t *, size_t), void (*lost)(void)) {
    ink_s31_uhci_buffer_t buffer;
    bool received = false;
    while (ink_s31_uhci_take(&buffer)) {
        received = true;
        if (feed) feed(buffer.data, buffer.len);
        ink_s31_uhci_return(buffer.token);
        if (ink_s31_uhci_recovered() && lost) lost();
    }
    return received;
}

/* unit-s31-hal-map: the S31 I2C error mapping and UART line assembly (ports/esp32s31/s31_hal_map.h). */
#include <assert.h>
#include <string.h>

#include "../../ports/esp32s31/s31_hal_map.h"

static int feed(s31_line_t *l, const char *s, int *last) { /* lines completed; *last = length of the last one */
    int lines = 0;
    for (; *s; s++) {
        int n = s31_line_feed(l, *s);
        if (n >= 0) {
            lines++;
            *last = n;
        }
    }
    return lines;
}

int main(void) {
    assert(s31_i2c_status(ESP_OK) == HAL_I2C_OK);
    assert(s31_i2c_status(ESP_ERR_INVALID_RESPONSE) == HAL_I2C_NACK);
    assert(s31_i2c_status(ESP_ERR_TIMEOUT) == HAL_I2C_TIMEOUT);
    assert(s31_i2c_status(ESP_ERR_INVALID_STATE) == HAL_I2C_ERR);
    assert(s31_i2c_status(ESP_FAIL) == HAL_I2C_ERR);

    s31_line_t l = {0};
    int last = -1;
    assert(feed(&l, "\r\nAT+GVER=V1.03\r\n", &last) == 1); /* the empty line before it is skipped */
    assert(last == 13 && !strcmp(l.buf, "AT+GVER=V1.03"));
    assert(feed(&l, "OK", &last) == 0);                    /* cut by a timeout: kept */
    assert(feed(&l, "\r\n", &last) == 1 && last == 2 && !strcmp(l.buf, "OK"));
    assert(feed(&l, "a\nb\n", &last) == 2 && !strcmp(l.buf, "b"));
    char longline[S31_LINE_CAP + 40];
    memset(longline, 'x', sizeof longline - 2);
    longline[sizeof longline - 2] = '\n';
    longline[sizeof longline - 1] = 0;
    assert(feed(&l, longline, &last) == 1 && last == S31_LINE_CAP - 1 && strlen(l.buf) == S31_LINE_CAP - 1);
    assert(feed(&l, "next\n", &last) == 1 && !strcmp(l.buf, "next")); /* the overflow did not leak into it */
    return 0;
}

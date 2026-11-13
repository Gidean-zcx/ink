/* MOBI is not read on this device: Mogo's metadata dispatch gets "not supported". */
#pragma once
#include <esp_err.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
static inline esp_err_t book_mobi_metadata(const char *p, char *t, size_t tc, char *a, size_t ac) { (void)p; (void)t; (void)tc; (void)a; (void)ac; return ESP_ERR_NOT_FOUND; }
static inline esp_err_t book_mobi_metadata_cached(const char *p, char *t, size_t tc, char *a, size_t ac) { (void)p; (void)t; (void)tc; (void)a; (void)ac; return ESP_ERR_NOT_FOUND; }
static inline esp_err_t book_mobi_cover_bounded(const char *p, uint8_t **d, size_t *s, bool *png, size_t b) { (void)p; (void)d; (void)s; (void)png; (void)b; return ESP_ERR_NOT_FOUND; }

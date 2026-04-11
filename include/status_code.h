#pragma once

#include <cstdint>

constexpr uint32_t MAGIC_NUMBER = 0x54525043;

enum StatusCode {
    OK,
    NOT_FOUND_SERVICE,
    NOT_FOUND_HANDLER,
    INVAILD_REQUEST,
    INTERNAL_ERROR
};
// main/web_server.h —— 内嵌配置页与 JSON API。
#pragma once

#include "esp_err.h"

esp_err_t web_server_start(void);
void web_server_stop(void);

#include "logger.h"

int main() {
    init_log_system("logs/app.log");

    LOG_INFO("MainModule", "This is an info message.");

    return 0;
}
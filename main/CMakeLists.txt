idf_component_register(SRCS "main.c" "audio_player.c" "sd_card.c" "button_handler.c" "neopixel.c" "pcm_file.c" "json_parser.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver fatfs esp_adc freertos nvs_flash esp_timer)

# Explicit source list: adding an example/test file cannot add a second main.
SOURCES := \
    app/src/main.c \
    app/src/EventCheckers.c \
    config/GameConfig.c \
    services/src/GameSM.c \
    services/src/HeatSM.c \
    services/src/IngredientSM.c \
    services/src/PanDisplaySM.c \
    services/src/ScoreSM.c \
    services/src/StirSM.c \
    drivers/src/Dispenser.c \
    drivers/src/InputButtons.c \
    drivers/src/Joystick.c \
    drivers/src/LedPanels.c \
    drivers/src/Servos.c \
    drivers/src/Sound.c \
    drivers/src/Ultrasonic.c \
    drivers/reused/DM_Display.c \
    drivers/reused/FontStuff.c \
    drivers/reused/PIC32_SPI_HAL.c \
    vendor/es_framework/FrameworkSource/ES_CheckEvents.c \
    vendor/es_framework/FrameworkSource/ES_DeferRecall.c \
    vendor/es_framework/FrameworkSource/ES_Framework.c \
    vendor/es_framework/FrameworkSource/ES_LookupTables.c \
    vendor/es_framework/FrameworkSource/ES_Port.c \
    vendor/es_framework/FrameworkSource/ES_PostList.c \
    vendor/es_framework/FrameworkSource/ES_Queue.c \
    vendor/es_framework/FrameworkSource/ES_Timers.c \
    vendor/es_framework/FrameworkSource/circular_buffer_no_modulo_threadsafe.c \
    vendor/es_framework/FrameworkSource/dbprintf.c \
    vendor/es_framework/FrameworkSource/terminal.c

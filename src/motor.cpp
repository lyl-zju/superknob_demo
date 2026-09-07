/*
 * @Descripttion:
 * @version:
 * @Author: congsir
 * @Date: 2022-05-22 05:30:09
 * @LastEditors: wenzheng 565402462@qq.com
 * @LastEditTime: 2022-07-23 13:38:50
 */
#include <motor.h>
#include <main.h>
#include <SimpleFOC.h>
#include <display.h>
#include <BleCombo.h>
#include "ui_pages/gui_super_knob.h"
#include "bluetooth_mouse.h"

enum MotorWorkMode {
    MOTOR_MODE_KNOB,
    MOTOR_MODE_MUSIC,
    MOTOR_MODE_JUMP,
};

static KnobConfig super_knob_configs[] = {
    {0, 0, 10 * PI / 180, 0, 1, 1.1, "Unbounded\nNo detents"},
    {0, 0, 8.225806452 * PI / 180, 2.3, 0, 1.1, "Continuous dense\nStrong detents"},
    {256, 127, 1 * PI / 180, 1, 1, 1.1, "Fine values\nWith detents"},
    {256, 127, 1 * PI / 180, 0, 1, 1.1, "Fine values\nNo detents"},
    {2, 0, 60 * PI / 180, 1, 1, 0.55, "On/off\nStrong detent"},
    {0, 0, 0, 0, 0, 1, "Damping\nViscous, no return", 0.6f},
};

static const float DEAD_ZONE_DETENT_PERCENT = 0.2;
static const float DEAD_ZONE_RAD = 1 * _PI / 180;
static const float IDLE_VELOCITY_EWMA_ALPHA = 0.001;
static const float IDLE_VELOCITY_RAD_PER_SEC = 0.05;
static const uint32_t IDLE_CORRECTION_DELAY_MILLIS = 500;
static const float IDLE_CORRECTION_MAX_ANGLE_RAD = 5 * PI / 180;
static const float IDLE_CORRECTION_RATE_ALPHA = 0.0005;
static const float DAMP_VELOCITY_DEAD_ZONE = 0.15f; // 阻尼死区(rad/s)，低于此速度输出 0
static const float DAMP_VELOCITY_FILTER_ALPHA = 0.05f; // 阻尼测速低通系数（~20ms 时间常数），磨平 12 位传感器的量化尖峰

static const int MOTOR_PWM_A_PIN = 32;
static const int MOTOR_PWM_B_PIN = 33;
static const int MOTOR_PWM_C_PIN = 25;
static const int MOTOR_ENABLE_PIN = 12;
static const int MUSIC_PWM_CH_A = 0;
static const int MUSIC_PWM_CH_B = 1;
static const int MUSIC_PWM_CH_C = 2;
static const uint32_t MOTOR_FOC_PWM_FREQ = 25000;
static const uint32_t MUSIC_PWM_BASE_FREQ = 30000;
static const uint8_t MUSIC_PWM_RESOLUTION = 8;
static const uint8_t MOTOR_FOC_PWM_RESOLUTION = 10;
static const float MUSIC_FREQ_MULTIPLIER = 3.0f;

enum NoteFrequency {
    REST = 0,

    L1 = 262,
    L2 = 294,
    L3 = 330,
    L4 = 349,
    L5 = 392,
    L6 = 440,
    L7 = 494,

    M1 = 523,
    M2 = 587,
    M3 = 659,
    M4 = 698,
    M5 = 784,
    M6 = 880,
    M7 = 988,

    H1 = 1046,
    H2 = 1175,
    H3 = 1318,
    H4 = 1397,
    H5 = 1568,
    H6 = 1760,
    H7 = 1976
};

static const uint16_t MUSIC_MELODY[] = {
    L3, L3, L3,
    L5, L1,
    L2, L4,
    L3, L5,
    L6, L5, L4,
    L6, REST,

    L6, L7, L6,
    L5, L5,
    M1, L7, L6,
    L5, L4, L3,
    L6, L2, L3,

    L2, REST,
    L3, L3, L6,
    L6, L5, L5,
    L5, L5, M1,
    L7, L7, M1,

    M2, M1, L7, L6, L5,
    L2, L6, L7,
    M1,
    L3, L3, L3,
    L5, L1,
    L2, L4,

    L3,
    L3, L3, L3,
    L6, L4, L4,
    L2, L1,
    L2, REST,
    L6, L6, L7,

    M1, L7, L6,
    L5, L4,
    L3, L1,
    L6, L6, L6, L2,
    L5, L5, L5, M1,

    L4, L4, L4, L5,
    L3, L2, L2,
    L1, L2, L3,
    L5, L4, L3, L4,
    L5, L6, L7, M1,

    M2, M1, L7, L6, L5,
    L2, L6, L7,
    M1, REST,
    L3, L3, L3,
    L5, L1,
    L2, L4,

    L3, L5,
    L5, L5,
    L6, L7,
    M1, L7, L6,
    L5, L5,
    M1, L7, L6,

    L5, L3,
    L6, L2, L3,
    L2, L2,
    L3, L2, L3,
    L4, L3,
    L4, L3, L4,

    L5, L4,
    L5, L4, L5,
    L6, L5,
    L6, L5, L6,
    L7, REST,

    M2, M1, L7, L6, L5, L4,
    L3, REST,
    L2, L6, L7,
    M1, REST
};

static const uint16_t MUSIC_DURATION_MS[] = {
    500, 500, 500,
    1000, 500,
    1000, 500,
    1000, 500,
    500, 500, 500,
    1000, 500,

    500, 500, 500,
    1000, 500,
    1000, 250, 250,
    750, 250, 500,
    500, 500, 500,

    1000, 500,
    500, 500, 500,
    750, 250, 500,
    500, 500, 500,
    1000, 250, 250,

    250, 250, 250, 250, 500,
    500, 500, 500,
    1500,
    500, 500, 500,
    1000, 500,
    1000, 500,

    1500,
    500, 500, 500,
    1000, 250, 250,
    1000, 500,
    1000, 500,
    500, 500, 500,

    1000, 250, 250,
    1000, 500,
    1000, 500,
    250, 250, 500, 500,
    250, 250, 500, 500,

    250, 250, 500, 500,
    250, 250, 1000,
    500, 500, 500,
    500, 500, 250, 250,
    500, 500, 250, 250,

    250, 250, 250, 250, 500,
    500, 500, 500,
    1000, 500,
    500, 500, 500,
    1000, 500,
    1000, 500,

    1000, 500,
    1000, 500,
    1000, 500,
    500, 500, 500,
    1000, 500,
    1000, 250, 250,

    1000, 500,
    500, 500, 500,
    1000, 500,
    750, 250, 500,
    1000, 500,
    750, 250, 500,

    1000, 500,
    750, 250, 500,
    1000, 500,
    750, 250, 500,
    1000, 500,

    250, 250, 250, 250, 250, 250,
    1000, 500,
    500, 500, 500,
    1000, 500
};

static const int MUSIC_NOTE_COUNT = sizeof(MUSIC_MELODY) / sizeof(MUSIC_MELODY[0]);
static_assert(MUSIC_NOTE_COUNT == sizeof(MUSIC_DURATION_MS) / sizeof(MUSIC_DURATION_MS[0]), "Music melody and duration length mismatch");

static MotorWorkMode motor_work_mode = MOTOR_MODE_KNOB;
static int music_note_index = 0;
static uint32_t music_note_start_ms = 0;

// Jump-game spring state. The FOC task writes it and the LVGL task reads it.
static portMUX_TYPE jump_state_mux = portMUX_INITIALIZER_UNLOCKED;
static float jump_charge_ratio = 0.0f;
static float jump_release_ratio = 0.0f;
static bool jump_release_pending = false;

static const float JUMP_MAX_ANGLE_RAD = 100.0f * PI / 180.0f;
static const float JUMP_MIN_RELEASE_RAD = 8.0f * PI / 180.0f;
static const float JUMP_RELEASE_DROP_RAD = 3.0f * PI / 180.0f;
static const float JUMP_SPRING_STRENGTH = 3.2f;
static const float JUMP_SPRING_DAMPING = 0.08f;

extern int current_os_mode;

KnobConfig motor_config = {
    .num_positions = 0,
    .position = 0,
    .position_width_radians = 8.225806452 * _PI / 180,
    .detent_strength_unit = 2.3,
    .endstop_strength_unit = 0,
    .snap_point = 1.1,
};

// Sensor and motor setupMagneticSensorI2C sensor = MagneticSensorI2C(0x36, 12, 0x0E, 4);
MagneticSensorI2C sensor = MagneticSensorI2C(AS5600_I2C);
TwoWire I2Cone = TwoWire(0);
//闁荤姳绀佹晶浠嬫偪閸℃稒鍋ㄩ柛顭戝亝缁?闂佸搫顑勭粈渚€顢氶鈧弫?
BLDCMotor motor = BLDCMotor(7);
//闁荤姳绀佹晶浠嬫偪閸℃ǜ浠氶柛妤冨仜琚熼梺?闂佹寧绋戦悧濠偽涢妶澶婄濡炲瀛╃痪顖涚箾缂堢姾鍏岄柡浣规倐瀵敻骞侀幒鍡椾壕濞达綀顕栭崝鍕级閳哄倻銆掗柡浣规倐瀵敻鎮€靛摜顦?// BLDCDriver3PWM( pin_pwmA, pin_pwmB, pin_pwmC, enable婵炶揪缍€閸庡宕楀Ο渚殨闁哄洠鈧啿顥涢梺鎸庣☉閻楀棜銇愰弻銉︾劵濠㈣泛鏈悾閬嶆煥?
// BLDCDriver3PWM driver = BLDCDriver3PWM(26, 27, 14, 12);
BLDCDriver3PWM driver = BLDCDriver3PWM(MOTOR_PWM_A_PIN, MOTOR_PWM_B_PIN, MOTOR_PWM_C_PIN, MOTOR_ENABLE_PIN);

static void setup_music_pwm()
{
    pinMode(MOTOR_ENABLE_PIN, OUTPUT);
    digitalWrite(MOTOR_ENABLE_PIN, HIGH);

    ledcSetup(MUSIC_PWM_CH_A, MUSIC_PWM_BASE_FREQ, MUSIC_PWM_RESOLUTION);
    ledcSetup(MUSIC_PWM_CH_B, MUSIC_PWM_BASE_FREQ, MUSIC_PWM_RESOLUTION);
    ledcSetup(MUSIC_PWM_CH_C, MUSIC_PWM_BASE_FREQ, MUSIC_PWM_RESOLUTION);
    ledcAttachPin(MOTOR_PWM_A_PIN, MUSIC_PWM_CH_A);
    ledcAttachPin(MOTOR_PWM_B_PIN, MUSIC_PWM_CH_B);
    ledcAttachPin(MOTOR_PWM_C_PIN, MUSIC_PWM_CH_C);
}

static void set_music_tone(uint16_t note_freq)
{
    uint32_t output_freq = 0;
    if (note_freq != REST)
    {
        output_freq = (uint32_t)(note_freq * MUSIC_FREQ_MULTIPLIER);
    }

    ledcWriteTone(MUSIC_PWM_CH_A, output_freq);
    ledcWriteTone(MUSIC_PWM_CH_B, output_freq);
    ledcWriteTone(MUSIC_PWM_CH_C, output_freq);
}

static void stop_music_tone()
{
    ledcWriteTone(MUSIC_PWM_CH_A, 0);
    ledcWriteTone(MUSIC_PWM_CH_B, 0);
    ledcWriteTone(MUSIC_PWM_CH_C, 0);
    ledcWrite(MUSIC_PWM_CH_A, 0);
    ledcWrite(MUSIC_PWM_CH_B, 0);
    ledcWrite(MUSIC_PWM_CH_C, 0);
}

static void reboot_to_main_after_music()
{
    stop_music_tone();

    ledcDetachPin(MOTOR_PWM_A_PIN);
    ledcDetachPin(MOTOR_PWM_B_PIN);
    ledcDetachPin(MOTOR_PWM_C_PIN);

    pinMode(MOTOR_PWM_A_PIN, OUTPUT);
    pinMode(MOTOR_PWM_B_PIN, OUTPUT);
    pinMode(MOTOR_PWM_C_PIN, OUTPUT);
    digitalWrite(MOTOR_PWM_A_PIN, LOW);
    digitalWrite(MOTOR_PWM_B_PIN, LOW);
    digitalWrite(MOTOR_PWM_C_PIN, LOW);

    pinMode(MOTOR_ENABLE_PIN, OUTPUT);
    digitalWrite(MOTOR_ENABLE_PIN, LOW);

    current_os_mode = 0;
    Serial.println("Music mode: reboot to restore FOC");
    Serial.flush();
    delay(100);
    ESP.restart();
}

static void restore_foc_pwm()
{
    ledcSetup(MUSIC_PWM_CH_A, MOTOR_FOC_PWM_FREQ, MOTOR_FOC_PWM_RESOLUTION);
    ledcSetup(MUSIC_PWM_CH_B, MOTOR_FOC_PWM_FREQ, MOTOR_FOC_PWM_RESOLUTION);
    ledcSetup(MUSIC_PWM_CH_C, MOTOR_FOC_PWM_FREQ, MOTOR_FOC_PWM_RESOLUTION);
    ledcAttachPin(MOTOR_PWM_A_PIN, MUSIC_PWM_CH_A);
    ledcAttachPin(MOTOR_PWM_B_PIN, MUSIC_PWM_CH_B);
    ledcAttachPin(MOTOR_PWM_C_PIN, MUSIC_PWM_CH_C);
}

static void enter_music_mode()
{
    motor.move(0);
    motor.loopFOC();

    setup_music_pwm();
    music_note_index = 0;
    music_note_start_ms = millis();
    motor_work_mode = MOTOR_MODE_MUSIC;
    set_music_tone(MUSIC_MELODY[music_note_index]);

    Serial.println("Music mode: play custom melody");
}

static void exit_music_mode()
{
    stop_music_tone();
    restore_foc_pwm();

    motor.enable();
    digitalWrite(MOTOR_ENABLE_PIN, HIGH);
    motor.controller = MotionControlType::torque;
    motor.voltage_limit = volt_limit;
    for (int i = 0; i < 20; i++)
    {
        motor.loopFOC();
        motor.move(0);
        vTaskDelay(1);
    }

    motor_work_mode = MOTOR_MODE_KNOB;
    Serial.println("Music mode: stop");
}

static bool run_music_logic()
{
    uint32_t now_ms = millis();
    if (now_ms - music_note_start_ms < MUSIC_DURATION_MS[music_note_index])
    {
        return false;
    }

    music_note_index++;
    if (music_note_index >= MUSIC_NOTE_COUNT)
    {
        return true;
    }

    music_note_start_ms = now_ms;
    set_music_tone(MUSIC_MELODY[music_note_index]);
    return false;
}

void init_angle(void)
{
    float target_angle = 0;
    target_angle = sensor.getAngle();
    float delta = volt_limit / init_smooth;
    for (int i = 0; i <= init_smooth; i++)
    {
        motor.voltage_limit = delta * i;
        motor.loopFOC();
        motor.move(target_angle);
    }
    motor.voltage_limit = volt_limit;
}

float CLAMP(const float value, const float low, const float high)
{
    return value < low ? low : (value > high ? high : value);
}

void motor_shake(int strength, int delay_time)
{
    motor.move(strength);
    for (int i = 0; i < delay_time; i++)
    {
        motor.loopFOC();
        vTaskDelay(1);
    }
    motor.move(-strength);
    for (int i = 0; i < delay_time; i++)
    {
        motor.loopFOC();
        vTaskDelay(1);
    }
}

int get_motor_position(void)
{
    return motor_config.position;
}

float get_motor_shaft_angle(void)
{
    return motor.shaft_angle;
}

float get_jump_charge_percent(void)
{
    portENTER_CRITICAL(&jump_state_mux);
    float percent = jump_charge_ratio * 100.0f;
    portEXIT_CRITICAL(&jump_state_mux);
    return percent;
}

bool consume_jump_release(float *charge_ratio)
{
    bool released;
    portENTER_CRITICAL(&jump_state_mux);
    released = jump_release_pending;
    if (released)
    {
        if (charge_ratio != NULL)
            *charge_ratio = jump_release_ratio;
        jump_release_pending = false;
    }
    portEXIT_CRITICAL(&jump_state_mux);
    return released;
}

void update_motor_status(MOTOR_RUNNING_MODE_E motor_status)
{
    struct _knob_message *send_message;
    send_message = &MOTOR_MSG;
    send_message->ucMessageID = motor_status;
    xQueueSend(motor_msg_Queue, &send_message, (TickType_t)0);
}

void update_motor_config(int status)
{
    motor_config = super_knob_configs[status];
}

extern int current_os_mode;
void Task_foc(void *pvParameters)
{
    (void)pvParameters;

    update_motor_status(MOTOR_INIT);
    I2Cone.begin(23, 5, 400000UL);
    sensor.init(&I2Cone);
    motor.linkSensor(&sensor);

    driver.voltage_power_supply = 12;
    driver.init();
    motor.linkDriver(&driver);

    motor.voltage_sensor_align = 3;
    motor.foc_modulation = FOCModulationType::SpaceVectorPWM;
    motor.controller = MotionControlType::angle;

    motor.PID_velocity.P = 0.08;
    motor.PID_velocity.I = 4;
    motor.PID_velocity.D = 0.0002;
    motor.LPF_velocity.Tf = 0.02;
    motor.P_angle.P = 20;
    motor.velocity_limit = 5;

    motor.useMonitoring(Serial);
    motor.init();
    motor.initFOC();
    update_motor_status(MOTOR_INIT_SUCCESS);

    init_angle();
    motor.controller = MotionControlType::torque;
    motor.loopFOC();
    update_motor_status(MOTOR_INIT_END);

    float current_detent_center = 0;
    uint32_t last_idle_start = 0;
    float idle_check_velocity_ewma = 0;
    float damp_vel_ewma = 0; // 阻尼模式的滤波后测速
    float jump_center = 0.0f;
    float jump_peak_angle = 0.0f;
    bool jump_fired = false;
    uint32_t jump_centered_since = 0;

    auto reset_knob_tracking = [&]() {
        current_detent_center = motor.shaft_angle;
        last_idle_start = 0;
        idle_check_velocity_ewma = 0;
        damp_vel_ewma = 0;
    };

    auto apply_detent_settings = [&]() {
        const float derivative_lower_strength = motor_config.detent_strength_unit * 0.08;
        const float derivative_upper_strength = motor_config.detent_strength_unit * 0.02;
        const float derivative_position_width_lower = radians(3);
        const float derivative_position_width_upper = radians(8);
        const float raw = derivative_lower_strength + (derivative_upper_strength - derivative_lower_strength) / (derivative_position_width_upper - derivative_position_width_lower) * (motor_config.position_width_radians - derivative_position_width_lower);
        motor.PID_velocity.D = CLAMP(
            raw,
            min(derivative_lower_strength, derivative_upper_strength),
            max(derivative_lower_strength, derivative_upper_strength));
        motor.PID_velocity.limit = 3;
        motor.PID_velocity.P = motor_config.detent_strength_unit * 4;
    };

    for (;;)
    {
        struct _knob_message *lvgl_message;
        if (xQueueReceive(motor_rcv_Queue, &(lvgl_message), (TickType_t)0))
        {
            Serial.print("motor_rcv_Queue --->");
            Serial.println(lvgl_message->ucMessageID);
            switch (lvgl_message->ucMessageID)
            {
            case CHECKOUT_PAGE:
            {
                if (motor_work_mode == MOTOR_MODE_MUSIC)
                {
                    reboot_to_main_after_music();
                }

                current_detent_center = motor.shaft_angle;
                damp_vel_ewma = 0;

                apply_detent_settings();

                motor_shake(2, 2);
            }
            break;
            case BUTTON_CLICK:
                if (motor_work_mode == MOTOR_MODE_MUSIC)
                {
                    reboot_to_main_after_music();
                }
                motor_shake(2, 2);
                break;
            case MUSIC_PLAY:
                enter_music_mode();
                reset_knob_tracking();
                break;
            case MUSIC_STOP:
                if (motor_work_mode == MOTOR_MODE_MUSIC)
                {
                    reboot_to_main_after_music();
                }
                break;
            case JUMP_SPRING_START:
                jump_center = motor.shaft_angle;
                jump_peak_angle = 0.0f;
                jump_fired = false;
                jump_centered_since = 0;
                motor_work_mode = MOTOR_MODE_JUMP;
                portENTER_CRITICAL(&jump_state_mux);
                jump_charge_ratio = 0.0f;
                jump_release_ratio = 0.0f;
                jump_release_pending = false;
                portEXIT_CRITICAL(&jump_state_mux);
                Serial.println("Jump mode: spring centered");
                break;
            default:
                break;
            }
        }

        if (current_os_mode == 1)
        {
            motor.loopFOC();
            run_pc_mouse_logic(motor.shaft_angle, motor.shaft_velocity);

            static int touch_check_cnt = 0;
            static bool pc_touch_ready = false;
            if (touch_check_cnt++ > 100)
            {
                touch_check_cnt = 0;
                const uint16_t touch_value = touchRead(ESP32_TOUCH_PIN1);

                // Require a release after boot and after each switch, so a
                // long touch cannot advance through several modes.
                if (touch_value > 18)
                    pc_touch_ready = true;
                else if (touch_value < 12 && pc_touch_ready)
                {
                    pc_touch_ready = false;
                    next_pc_control_mode(motor.shaft_angle);
                }
            }
        }
        else if (motor_work_mode == MOTOR_MODE_MUSIC)
        {
            if (run_music_logic())
            {
                reboot_to_main_after_music();
            }
        }
        else if (motor_work_mode == MOTOR_MODE_JUMP)
        {
            motor.loopFOC();

            const float delta = motor.shaft_angle - jump_center;
            const float abs_delta = fabsf(delta);
            const float ratio = CLAMP(abs_delta / JUMP_MAX_ANGLE_RAD, 0.0f, 1.0f);

            if (!jump_fired && abs_delta > jump_peak_angle)
                jump_peak_angle = abs_delta;

            // Moving clearly back towards the centre is treated as releasing the knob.
            const bool moving_home = delta * motor.shaft_velocity < -0.04f;
            if (!jump_fired && jump_peak_angle >= JUMP_MIN_RELEASE_RAD &&
                jump_peak_angle - abs_delta >= JUMP_RELEASE_DROP_RAD && moving_home)
            {
                jump_fired = true;
                const float released_ratio = CLAMP(jump_peak_angle / JUMP_MAX_ANGLE_RAD, 0.0f, 1.0f);
                portENTER_CRITICAL(&jump_state_mux);
                jump_release_ratio = released_ratio;
                jump_release_pending = true;
                portEXIT_CRITICAL(&jump_state_mux);
            }

            if (jump_fired && abs_delta < radians(3.0f))
            {
                if (jump_centered_since == 0)
                    jump_centered_since = millis();
                else if (millis() - jump_centered_since >= 100)
                {
                    jump_fired = false;
                    jump_peak_angle = 0.0f;
                    jump_center = motor.shaft_angle;
                    jump_centered_since = 0;
                }
            }
            else
                jump_centered_since = 0;

            portENTER_CRITICAL(&jump_state_mux);
            jump_charge_ratio = ratio;
            portEXIT_CRITICAL(&jump_state_mux);

            float torque = -JUMP_SPRING_STRENGTH * delta - JUMP_SPRING_DAMPING * motor.shaft_velocity;
            if (abs_delta > JUMP_MAX_ANGLE_RAD)
                torque += -copysignf((abs_delta - JUMP_MAX_ANGLE_RAD) * 10.0f, delta);
            motor.move(CLAMP(torque, -5.5f, 5.5f));
        }
        else
        {
            motor.loopFOC();

            if (motor_config.damping_strength > 0.0f)
            {
                // 纯粘性阻尼：反向力矩 ∝ 角速度。转动时越转越快阻力越大（像弹簧），
                // 但没有回中力，松手后停在哪就停在哪，不会一直转回初始位置。
                // 传感器 12 位差分测速有量化噪声（1 LSB ≈ 1.5 rad/s 尖峰），直接反馈
                // 会把噪声放大成持续抖动/机械噪音：先低通滤波磨平，再进死区彻底消除静止抖动。
                damp_vel_ewma = damp_vel_ewma * (1.0f - DAMP_VELOCITY_FILTER_ALPHA)
                                + motor.shaft_velocity * DAMP_VELOCITY_FILTER_ALPHA;
                float torque = 0.0f;
                if (fabsf(damp_vel_ewma) > DAMP_VELOCITY_DEAD_ZONE)
                {
                    torque = -motor_config.damping_strength * damp_vel_ewma;
                    torque = CLAMP(torque, -3.0f, 3.0f);
                }
                motor.move(torque);
                vTaskDelay(1);
                continue;
            }

            idle_check_velocity_ewma = motor.shaft_velocity * IDLE_VELOCITY_EWMA_ALPHA + idle_check_velocity_ewma * (1 - IDLE_VELOCITY_EWMA_ALPHA);
            if (fabsf(idle_check_velocity_ewma) > IDLE_VELOCITY_RAD_PER_SEC)
            {
                last_idle_start = 0;
            }
            else if (last_idle_start == 0)
            {
                last_idle_start = millis();
            }

            if (last_idle_start > 0 && millis() - last_idle_start > IDLE_CORRECTION_DELAY_MILLIS && fabsf(motor.shaft_angle - current_detent_center) < IDLE_CORRECTION_MAX_ANGLE_RAD)
            {
                current_detent_center = motor.shaft_angle * IDLE_CORRECTION_RATE_ALPHA + current_detent_center * (1 - IDLE_CORRECTION_RATE_ALPHA);
            }

            float angle_to_detent_center = motor.shaft_angle - current_detent_center;

            if (angle_to_detent_center > motor_config.position_width_radians * motor_config.snap_point && (motor_config.num_positions <= 0 || motor_config.position > 0))
            {
                current_detent_center += motor_config.position_width_radians;
                angle_to_detent_center -= motor_config.position_width_radians;
                motor_config.position--;
            }
            else if (angle_to_detent_center < -motor_config.position_width_radians * motor_config.snap_point && (motor_config.num_positions <= 0 || motor_config.position < motor_config.num_positions - 1))
            {
                current_detent_center -= motor_config.position_width_radians;
                angle_to_detent_center += motor_config.position_width_radians;
                motor_config.position++;
            }

            float dead_zone_adjustment = CLAMP(
                angle_to_detent_center,
                fmaxf(-motor_config.position_width_radians * DEAD_ZONE_DETENT_PERCENT, -DEAD_ZONE_RAD),
                fminf(motor_config.position_width_radians * DEAD_ZONE_DETENT_PERCENT, DEAD_ZONE_RAD));

            bool out_of_bounds = motor_config.num_positions > 0 && ((angle_to_detent_center > 0 && motor_config.position == 0) || (angle_to_detent_center < 0 && motor_config.position == motor_config.num_positions - 1));
            motor.PID_velocity.limit = out_of_bounds ? 10 : 3;
            motor.PID_velocity.P = out_of_bounds ? motor_config.endstop_strength_unit * 4 : motor_config.detent_strength_unit * 4;

            if (fabsf(motor.shaft_velocity) > 60)
            {
                Serial.println("(motor.shaft_velocity) > 60 !!!");
                motor.move(0);
            }
            else
            {
                float torque = motor.PID_velocity(-angle_to_detent_center + dead_zone_adjustment);
                motor.move(torque);
            }
        }

        vTaskDelay(1);
    }
}

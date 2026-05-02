/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <math.h>
#include <string.h>

/* Private define ------------------------------------------------------------*/
#define COUNTS_PER_REV    2000      /* 500 PPR x 4 quadrature    */
#define CIRCUMFERENCE_CM  10.0f     /* cm per full revolution     */
#define SAMPLE_MS         100       /* RPM sampling window in ms  */
#define MIN_DELTA         2         /* noise threshold in counts  */
#define EMA_ALPHA         0.5f      /* EMA filter coefficient     */

/* Private variables ---------------------------------------------------------*/
int32_t g_total_counts = 0;
int32_t g_prev_count   = 0;
float   g_rpm_filtered = 0.0f;

/* Private function prototypes -----------------------------------------------*/
void Send_Both(const char *str, uint16_t len);
void Encoder_Process(void);

/* Private user code ---------------------------------------------------------*/
void Send_Both(const char *str, uint16_t len)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)str, len, 100);  // → external device
    HAL_UART_Transmit(&huart2, (uint8_t *)str, len, 100);  // → serial monitor
}

/* -- Start encoder ----------------------------------------- */
HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);   //  Encoder start
HAL_Delay(1000);
__HAL_TIM_SET_COUNTER(&htim2, 0);
g_prev_count_R = 0;
g_total_counts_R = 0;

* -- Startup message --------------------------------------- */
char msg[] = "\r\n=== Autonics E50S8-500-3-T-1 ===\r\n";
Send_Both(msg, strlen(msg));

/* -- Main loop --------------------------------------------- */
uint32_t last_tick = HAL_GetTick();

while(1)
{
    if ((HAL_GetTick() - last_tick) >= SAMPLE_MS)
	     {
	        last_tick = HAL_GetTick();
	        Encoder_Process();
	     }
}

void Encoder_Process(void)
{
    char     buf[80];
    uint16_t len;
/* ── 1. Read & overflow-correct counter ─────────────────────── */
    int32_t current = (int32_t)__HAL_TIM_GET_COUNTER(&htim2);  // ← TIM2

    int32_t delta = current - g_prev_count;                   
    if      (delta >  32767) delta -= 65536;
    else if (delta < -32768) delta += 65536;
 /* ── KEY CHANGE: Negate delta so CCW = +ve, CW = -ve ────────── */
    // Remove or keep negation depending on your wiring direction
     delta = -delta;

    if (delta > -MIN_DELTA && delta < MIN_DELTA)
        delta = 0;
/* Dead-band */
    g_prev_count    = current;                                
    g_total_counts += delta;                                  
/* ── 2. Raw RPM ──────────────────────────────────────────────── */
    float sample_sec = (float)SAMPLE_MS / 1000.0f;
    float rpm_raw    = (fabsf((float)delta) / (float)COUNTS_PER_REV)
                       / sample_sec * 60.0f;
 /* ── 3. EMA filter ───────────────────────────────────────────── */
    #define EMA_ALPHA  0.5f

    if (delta == 0)
        g_rpm_filtered = g_rpm_filtered * (1.0f - EMA_ALPHA); // ← _R
    else
        g_rpm_filtered = EMA_ALPHA * rpm_raw + (1.0f - EMA_ALPHA) * g_rpm_filtered;
/* ── 4. Distance (CCW +, CW −) ───────────────────────────────── */
    float distance_cm = ((float)g_total_counts / (float)COUNTS_PER_REV)
                        * CIRCUMFERENCE_CM;                     
/* ── 5. Direction tag ────────────────────────────────────────── */
    /* NOTE: After negation, delta>0 means CCW, delta<0 means CW     */
    const char *dir = (delta > 0) ? "CW " : (delta < 0) ? "CCW" : "---";
/* ── 6. Transmit ─────────────────────────────────────────────── */
    len = snprintf(buf, sizeof(buf),
                   "R[%s] RPM: %7.2f  |  Distance: %9.3f cm\r\n",  // ← "R"
                   dir,
                   (double)g_rpm_filtered,
                   (double)distance_cm);
    Send_Both(buf, len);
}



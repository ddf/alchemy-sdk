/**
 * @file ws2812.cpp
 * @brief alchemy::Ws2812Strip — TIM3_CH4 PWM + DMA1 Stream 7.
 *
 * See alchemy/hw/ws2812.h for hardware requirements and usage.
 *
 * Each WS2812 bit is encoded as one PWM cycle on TIM3 channel 4:
 *
 *      ┌──────┐                    ┌──────┐
 *  "0" │      │_______________     │ T0H  │___T0L___
 *      ▔                    ▔
 *      ┌─────────┐                 ┌─────────┐
 *  "1" │         │__________       │   T1H   │_T1L_
 *      ▔                  ▔
 *
 * For a 1.25 µs bit period (800 kHz):
 *   T0H ≈ 0.40 µs   (CCR ≈ 0.32 × ARR)
 *   T1H ≈ 0.80 µs   (CCR ≈ 0.64 × ARR)
 *
 * DMA streams CCR half-words from a DMA-accessible buffer into TIM3->CCR4
 * on every TIM3 update event.  The buffer tail is zero-filled for ≥280 µs
 * latch time.
 *
 * Resource usage:
 *   - TIM3 channel 4
 *   - DMA1 Stream 7, DMA_REQUEST_TIM3_UP
 *   - GPIO PC9 (AF2 = TIM3_CH4)
 */

#include "alchemy/hw/ws2812.h"
#include "daisy_core.h"       /* DMA_BUFFER_MEM_SECTION */
#include "stm32h7xx_hal.h"
#include <cstring>

/* ── Bit timing constants ────────────────────────────────────────────────── */

static constexpr uint32_t kBitsPerLed = 24u;    /* 3 channels × 8 bits */
static constexpr uint32_t kBitFreqHz  = 800000u; /* WS2812 wire frequency */
static constexpr float    kT0HFrac    = 0.32f;   /* ≈ 0.40 µs */
static constexpr float    kT1HFrac    = 0.64f;   /* ≈ 0.80 µs */

/** Reset latch: 240 cells × 1.25 µs = 300 µs (datasheet min: 280 µs). */
static constexpr uint16_t kResetCells = 240u;

static constexpr uint32_t kMaxCells =
    static_cast<uint32_t>(kWs2812MaxLeds) * kBitsPerLed + kResetCells;

static uint16_t DMA_BUFFER_MEM_SECTION ccr_buf_[kMaxCells];

static TIM_HandleTypeDef htim3_;
static DMA_HandleTypeDef hdma_tim3_up_;

static uint16_t ccr_zero_ = 0;
static uint16_t ccr_one_  = 0;
static uint16_t xfer_len_ = 0;

static volatile bool xfer_busy_ = false;

/* ── Forward declarations ────────────────────────────────────────────────── */

static void     InitGpio();
static void     InitTim(uint32_t period_ticks);
static void     InitDma();
static uint32_t TimerKernelHz();
static void     EncodePixels(const uint8_t* pixels, uint16_t num_leds);
static void     DmaTcCb (DMA_HandleTypeDef* hdma);
static void     DmaErrCb(DMA_HandleTypeDef* hdma);

/* ========================================================================= */
/*  Ws2812Strip public API                                                    */
/* ========================================================================= */

namespace alchemy {

void Ws2812Strip::Init(uint16_t num_leds)
{
    if (num_leds > kWs2812MaxLeds)
        num_leds = kWs2812MaxLeds;

    num_leds_ = num_leds;
    xfer_len_ = static_cast<uint16_t>(num_leds_ * kBitsPerLed + kResetCells);

    std::memset(pixel_buf_, 0, sizeof(pixel_buf_));
    std::memset(ccr_buf_,   0, sizeof(ccr_buf_));

    const uint32_t tim_hz       = TimerKernelHz();
    const uint32_t period_ticks = (tim_hz + (kBitFreqHz / 2)) / kBitFreqHz;

    ccr_zero_ = static_cast<uint16_t>(
        static_cast<float>(period_ticks) * kT0HFrac + 0.5f);
    ccr_one_  = static_cast<uint16_t>(
        static_cast<float>(period_ticks) * kT1HFrac + 0.5f);

    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    InitGpio();
    InitDma();
    InitTim(period_ticks);

    __HAL_TIM_ENABLE(&htim3_);
}

void Ws2812Strip::SetPixel(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= num_leds_) return;
    const uint16_t off = index * 3;
    pixel_buf_[off + 0] = g;
    pixel_buf_[off + 1] = r;
    pixel_buf_[off + 2] = b;
}

void Ws2812Strip::Clear()
{
    std::memset(pixel_buf_, 0, sizeof(pixel_buf_));
}

void Ws2812Strip::Show()
{
    while (xfer_busy_)
        ;

    EncodePixels(pixel_buf_, num_leds_);

    __HAL_DMA_DISABLE(&hdma_tim3_up_);

    xfer_busy_ = true;
    /* STM32H7 DMA uses 32-bit bus addresses; the double cast is the correct
     * portable way to convert a pointer to the uint32_t the HAL expects.
     * On the STM32 (32-bit), uintptr_t == uint32_t so no truncation occurs. */
    HAL_DMA_Start_IT(&hdma_tim3_up_,
                     static_cast<uintptr_t>(reinterpret_cast<uintptr_t>(ccr_buf_)),
                     static_cast<uintptr_t>(reinterpret_cast<uintptr_t>(&TIM3->CCR4)),
                     xfer_len_);

    __HAL_TIM_ENABLE_DMA(&htim3_, TIM_DMA_UPDATE);
}

bool Ws2812Strip::Busy() const
{
    return xfer_busy_;
}

} // namespace alchemy

static void InitGpio()
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {};
    gpio.Pin       = GPIO_PIN_9;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLDOWN;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOC, &gpio);
}

static void InitTim(uint32_t period_ticks)
{
    std::memset(&htim3_, 0, sizeof(htim3_));
    htim3_.Instance               = TIM3;
    htim3_.Init.Prescaler         = 0;
    htim3_.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3_.Init.Period            = period_ticks - 1;
    htim3_.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3_.Init.RepetitionCounter = 0;
    htim3_.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    htim3_.State                  = HAL_TIM_STATE_BUSY; /* skip MspInit */

    HAL_TIM_PWM_Init(&htim3_);

    TIM_OC_InitTypeDef oc = {};
    oc.OCMode       = TIM_OCMODE_PWM1;
    oc.Pulse        = 0;
    oc.OCPolarity   = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode   = TIM_OCFAST_DISABLE;
    oc.OCIdleState  = TIM_OCIDLESTATE_RESET;
    oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    HAL_TIM_PWM_ConfigChannel(&htim3_, &oc, TIM_CHANNEL_4);

    TIM3->CCMR2 |= TIM_CCMR2_OC4PE;
    TIM_CCxChannelCmd(TIM3, TIM_CHANNEL_4, TIM_CCx_ENABLE);
}

static void InitDma()
{
    std::memset(&hdma_tim3_up_, 0, sizeof(hdma_tim3_up_));
    hdma_tim3_up_.Instance                 = DMA1_Stream7;
    hdma_tim3_up_.Init.Request             = DMA_REQUEST_TIM3_UP;
    hdma_tim3_up_.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_tim3_up_.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_tim3_up_.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_tim3_up_.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_tim3_up_.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    hdma_tim3_up_.Init.Mode                = DMA_NORMAL;
    hdma_tim3_up_.Init.Priority            = DMA_PRIORITY_LOW;
    hdma_tim3_up_.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_tim3_up_);

    hdma_tim3_up_.XferCpltCallback     = DmaTcCb;
    hdma_tim3_up_.XferHalfCpltCallback = nullptr;
    hdma_tim3_up_.XferErrorCallback    = DmaErrCb;

    /* Priority 5: below audio DMA (0), above background tasks. */
    HAL_NVIC_SetPriority(DMA1_Stream7_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream7_IRQn);
}

static uint32_t TimerKernelHz()
{
    const uint32_t pclk1   = HAL_RCC_GetPCLK1Freq();
    const uint32_t d2ppre1 = (RCC->D2CFGR & RCC_D2CFGR_D2PPRE1)
                              >> RCC_D2CFGR_D2PPRE1_Pos;
    const uint32_t mult    = (d2ppre1 < 4) ? 1u : 2u;
    const uint32_t hz      = pclk1 * mult;
    return (hz != 0u) ? hz : 200000000u;
}

static void EncodePixels(const uint8_t* pixels, uint16_t num_leds)
{
    uint16_t*      dst         = ccr_buf_;
    const uint16_t total_bytes = static_cast<uint16_t>(num_leds * 3u);

    for (uint16_t i = 0; i < total_bytes; i++)
    {
        uint8_t byte = pixels[i];
        for (uint8_t b = 0; b < 8; b++)
        {
            *dst++ = (byte & 0x80u) ? ccr_one_ : ccr_zero_;
            byte <<= 1;
        }
    }
    for (uint16_t i = 0; i < kResetCells; i++)
        *dst++ = 0;
}

static void DmaTcCb(DMA_HandleTypeDef* /* hdma */)
{
    __HAL_TIM_DISABLE_DMA(&htim3_, TIM_DMA_UPDATE);
    xfer_busy_ = false;
}

static void DmaErrCb(DMA_HandleTypeDef* /* hdma */)
{
    __HAL_TIM_DISABLE_DMA(&htim3_, TIM_DMA_UPDATE);
    xfer_busy_ = false;
}

extern "C" void DMA1_Stream7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_tim3_up_);
}

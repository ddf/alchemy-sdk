/**
 * @file ws2812.cpp
 * @brief alchemy::Ws2812Strip for V2 — TIM6 update → DMA → GPIOD->BSRR.
 *
 * Why this driver exists:
 *   V2 routes WS2812 data on PD11, which has no TIM PWM channel on
 *   STM32H7 — V1's TIM3_CH4 + DMA-to-CCR pattern doesn't port. Instead,
 *   TIM6 generates an update event at 2.4 MHz and DMA1 Stream 7 writes a
 *   precomputed BSRR word on every event; the pin follows the pattern in
 *   plain push-pull output mode (no AF).
 *
 * Bit encoding (3 sub-cells per WS2812 bit, 417 ns each → 1.25 µs / bit):
 *
 *      cell 0     cell 1     cell 2
 *    ┌────────┬──────────┬────────┐
 *    │  HIGH  │   ???    │  LOW   │      ??? = HIGH for "1", LOW for "0"
 *    └────────┴──────────┴────────┘
 *      0 ns     417 ns     833 ns      1250 ns
 *
 *   "0" pulse width = 417 ns  (spec T0H 200–500 ns ✓)
 *   "1" pulse width = 833 ns  (spec T1H 580–1000 ns ✓)
 *
 */

#include "alchemy/hw/ws2812.h"
#include "stm32h7xx_hal.h"
#include "core_cm7.h"
#include <cstring>

/* ── Bit-encoding constants ──────────────────────────────────────────────── */

static constexpr uint32_t kBitsPerLed     = 24u;
static constexpr uint32_t kSubCellsPerBit = 3u;
static constexpr uint32_t kSubCellHz      = 2400000u; /* 3 × 800 kHz */

/** Reset latch: 720 cells × 417 ns ≈ 300 µs (datasheet min: 280 µs). */
static constexpr uint16_t kLatchCells = 720u;

static constexpr uint32_t kMaxCells =
    static_cast<uint32_t>(alchemy::kWs2812MaxLeds) * kBitsPerLed * kSubCellsPerBit
    + kLatchCells;

/* PD11 BSRR encodings — set / reset / no-op (zero) */
static constexpr uint32_t kBsrrPinHigh = (1u << 11);
static constexpr uint32_t kBsrrPinLow  = (1u << 11) << 16;
static constexpr uint32_t kBsrrNoOp    = 0u;

/* ── File-scope hardware state ───────────────────────────────────────────── */

/* .ws2812_bss is placed at 0x30008000 (start of RAM_D2) by the vendored
 * linker script, exactly covering MPU region 3 configured in Init().
 * Must fit the 32 KB region. */
static uint32_t __attribute__((section(".ws2812_bss"), aligned(32)))
    cell_buf_[kMaxCells];

static_assert(sizeof(cell_buf_) <= 32768u,
              "WS2812 cell buffer must fit the 32 KB non-cacheable MPU region");

static TIM_HandleTypeDef htim6_;
static DMA_HandleTypeDef hdma_tim6_up_;

static uint16_t xfer_len_ = 0;

/** Set by Show(); cleared by DMA transfer-complete ISR callback. */
static volatile bool xfer_busy_ = false;

/* ── Forward declarations ────────────────────────────────────────────────── */

static void     ConfigureBufferMpuRegion();
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
    /* Call before audio starts: the MPU region setup below briefly
     * disables the MPU, which must not race active DMA. The V2 BSP
     * Init() satisfies this (LEDs are step 8; StartAudio comes later). */
    if (num_leds > kWs2812MaxLeds)
        num_leds = kWs2812MaxLeds;

    num_leds_ = num_leds;
    xfer_len_ = static_cast<uint16_t>(
        num_leds_ * kBitsPerLed * kSubCellsPerBit + kLatchCells);

    /* Make the buffer non-cacheable BEFORE touching it, then purge any
     * cache lines that may already alias the range (e.g. from earlier
     * boot-time accesses) so they can never be evicted over fresh data. */
    ConfigureBufferMpuRegion();
    SCB_CleanInvalidateDCache_by_Addr(
        reinterpret_cast<uint32_t*>(cell_buf_),
        static_cast<int32_t>(sizeof(cell_buf_)));

    std::memset(pixel_buf_, 0, sizeof(pixel_buf_));
    std::memset(cell_buf_,  0, sizeof(cell_buf_));

    const uint32_t tim_hz       = TimerKernelHz();
    const uint32_t period_ticks = (tim_hz + (kSubCellHz / 2)) / kSubCellHz;

    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_TIM6_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    InitGpio();
    InitDma();
    InitTim(period_ticks);

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
    /* Wait for previous DMA, but bail after 50 ms. Don't hang. */
    for (uint32_t t = 0; t < 50u && xfer_busy_; ++t)
        daisy::System::Delay(1);
    if (xfer_busy_)
    {
        __HAL_TIM_DISABLE(&htim6_);
        __HAL_TIM_DISABLE_DMA(&htim6_, TIM_DMA_UPDATE);
        HAL_DMA_Abort(&hdma_tim6_up_);
        GPIOD->BSRR = kBsrrPinLow;
        xfer_busy_ = false;
    }

    EncodePixels(pixel_buf_, num_leds_);

    /* Buffer is in the non-cacheable MPU region: the writes above are
     * already in RAM. The barrier just orders them ahead of the DMA
     * enable that follows. */
    __DSB();

    __HAL_DMA_DISABLE(&hdma_tim6_up_);

    /* Reset TIM6 to a known phase before re-arming DMA so the first
     * cell always gets a full 417 ns period. */
    __HAL_TIM_DISABLE(&htim6_);
    __HAL_TIM_SET_COUNTER(&htim6_, 0);

    const HAL_StatusTypeDef rc = HAL_DMA_Start_IT(
        &hdma_tim6_up_,
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(cell_buf_)),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&GPIOD->BSRR)),
        xfer_len_);
    if (rc != HAL_OK)
    {
        HAL_DMA_Abort(&hdma_tim6_up_); /* clean stream state for retry */
        return;  /* xfer_busy_ stays false; caller can retry */
    }

    xfer_busy_ = true;
    __HAL_TIM_ENABLE_DMA(&htim6_, TIM_DMA_UPDATE);
    __HAL_TIM_ENABLE(&htim6_);
}

bool Ws2812Strip::Busy() const
{
    return xfer_busy_;
}

} // namespace alchemy

/* ========================================================================= */
/*  MPU, GPIO, TIM6, DMA init                                                */
/* ========================================================================= */

/** MPU region 3: 32 KB non-cacheable window at 0x30008000 covering the
 *  cell buffer. Identical attribute set to libDaisy's region 0 (its
 *  non-cacheable DMA buffer window at 0x30000000); regions 0–2 are owned
 *  by libDaisy, region 3 is free. */
static void ConfigureBufferMpuRegion()
{
    MPU_Region_InitTypeDef r = {};
    HAL_MPU_Disable();
    r.Enable           = MPU_REGION_ENABLE;
    r.BaseAddress      = 0x30008000u;
    r.Size             = MPU_REGION_SIZE_32KB;
    r.AccessPermission = MPU_REGION_FULL_ACCESS;
    r.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
    r.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
    r.IsShareable      = MPU_ACCESS_SHAREABLE;
    r.Number           = MPU_REGION_NUMBER3;
    r.TypeExtField     = MPU_TEX_LEVEL1;
    r.SubRegionDisable = 0x00;
    r.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
    HAL_MPU_ConfigRegion(&r);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

static void InitGpio()
{
    GPIO_InitTypeDef gpio = {};
    gpio.Pin       = GPIO_PIN_11;
    gpio.Mode      = GPIO_MODE_OUTPUT_PP;
    /* Pull-down keeps PD11 at the WS2812-defined idle level (low) between
     * board reset and the first Show() — without it the line floats and
     * the chain can latch garbage on cold start. */
    gpio.Pull      = GPIO_PULLDOWN;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOD, &gpio);
    /* Drive low before transmission so the chain stays idle. */
    GPIOD->BSRR = kBsrrPinLow;
}

static void InitTim(uint32_t period_ticks)
{
    std::memset(&htim6_, 0, sizeof(htim6_));
    htim6_.Instance               = TIM6;
    htim6_.Init.Prescaler         = 0;
    htim6_.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim6_.Init.Period            = period_ticks - 1;
    htim6_.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim6_.Init.RepetitionCounter = 0;
    htim6_.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    htim6_.State                  = HAL_TIM_STATE_BUSY; /* skip MspInit */

    HAL_TIM_Base_Init(&htim6_);
}

static void InitDma()
{
    std::memset(&hdma_tim6_up_, 0, sizeof(hdma_tim6_up_));
    hdma_tim6_up_.Instance                 = DMA1_Stream7;
    hdma_tim6_up_.Init.Request             = DMA_REQUEST_TIM6_UP;
    hdma_tim6_up_.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_tim6_up_.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_tim6_up_.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_tim6_up_.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_tim6_up_.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
    hdma_tim6_up_.Init.Mode                = DMA_NORMAL;
    /* HIGH (not VERY_HIGH): audio SAI DMA must always win. LOW would put
     * us behind casual mem-to-mem DMA — arbitration jitter then eats
     * into the 417 ns per-cell budget. */
    hdma_tim6_up_.Init.Priority            = DMA_PRIORITY_HIGH;
    /* FIFO + 4-beat memory burst: pre-fetch 4 words into the 16-word
     * FIFO, dispense one per timer update. Bus-contention transients
     * drain the FIFO instead of corrupting a wire bit. xfer_len_ is
     * always a multiple of 4 (n·72 + 720). */
    hdma_tim6_up_.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
    hdma_tim6_up_.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
    hdma_tim6_up_.Init.MemBurst            = DMA_MBURST_INC4;
    hdma_tim6_up_.Init.PeriphBurst         = DMA_PBURST_SINGLE;
    HAL_DMA_Init(&hdma_tim6_up_);

    hdma_tim6_up_.XferCpltCallback     = DmaTcCb;
    hdma_tim6_up_.XferHalfCpltCallback = nullptr;
    hdma_tim6_up_.XferErrorCallback    = DmaErrCb;

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
    uint32_t*      dst         = cell_buf_;
    const uint16_t total_bytes = static_cast<uint16_t>(num_leds * 3u);

    for (uint16_t i = 0; i < total_bytes; i++)
    {
        uint8_t byte = pixels[i];
        for (uint8_t b = 0; b < 8; b++)
        {
            const bool one = (byte & 0x80u) != 0;
            *dst++ = kBsrrPinHigh;
            *dst++ = one ? kBsrrNoOp : kBsrrPinLow;
            *dst++ = kBsrrPinLow;
            byte <<= 1;
        }
    }
    for (uint16_t i = 0; i < kLatchCells; i++)
        *dst++ = kBsrrNoOp;
}

static void DmaTcCb(DMA_HandleTypeDef* /* hdma */)
{
    __HAL_TIM_DISABLE_DMA(&htim6_, TIM_DMA_UPDATE);
    /* Guarantee pin is low at end of frame. */
    GPIOD->BSRR = kBsrrPinLow;
    xfer_busy_ = false;
}

static void DmaErrCb(DMA_HandleTypeDef* /* hdma */)
{
    __HAL_TIM_DISABLE_DMA(&htim6_, TIM_DMA_UPDATE);
    GPIOD->BSRR = kBsrrPinLow;
    xfer_busy_ = false;
}

extern "C" void DMA1_Stream7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_tim6_up_);
}

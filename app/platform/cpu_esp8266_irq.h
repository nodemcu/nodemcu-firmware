#pragma once

static inline uint32_t
esp8266_defer_irqs(void)
{
	uint32_t state;
	__asm__ __volatile__ ("rsil %0, 15" : "=a"(state) : : "memory");
	return state;
}

static inline void
esp8266_restore_irqs(uint32_t state)
{
	__asm__ __volatile__ ("wsr %0, ps" : : "a"(state) : "memory");
}

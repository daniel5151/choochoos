#ifdef __cplusplus
extern "C" {
#endif

void _swi_handler(void);
void _irq_handler(void);

void* _activate_task(void* next_sp);

void _enable_caches(void);
void _disable_caches(void);

#ifdef __cplusplus
}
#endif

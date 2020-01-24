#ifdef __cplusplus
extern "C" {
#endif

void _swi_handler();
void* _activate_task(void* next_sp);

void _enable_caches();
void _disable_caches();

#ifdef __cplusplus
}
#endif

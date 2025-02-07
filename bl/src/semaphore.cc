/*
 * Leid 2024.3
 */
#include "bl/semaphore.h"

#ifdef _WIN32
#define _WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <semaphore.h>
#endif

namespace bl {

Semaphore::Semaphore() {
#ifdef _WIN32
  _sem = CreateSemaphore(NULL, 0, 10, NULL);
#else
  sem_init((sem_t*)&_sem, 0, 0);
#endif
}

Semaphore ::~Semaphore() {
#ifdef _WIN32
  CloseHandle(_sem);
#else
  sem_destroy((sem_t*)&_sem);
#endif
}

int Semaphore::Wait() {
#ifdef _WIN32
  return (WAIT_OBJECT_0 == WaitForSingleObject(_sem, INFINITE));
#else
  return sem_wait((sem_t*)&_sem);
#endif
}

int Semaphore::Post() {
#ifdef _WIN32
  return (0 == ReleaseSemaphore(_sem, 1, nullptr));
#else
  return sem_post((sem_t*)&_sem);
#endif
}

}  // namespace bl

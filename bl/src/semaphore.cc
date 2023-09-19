/*
 * lujy 2021.6
 */
#include "bl/semaphore.h"
#include "semaphore.h"

namespace bl {

Semaphore::Semaphore() {
  sem_init((sem_t*)&_sem, 0, 0);
}

Semaphore ::~Semaphore() {
  sem_destroy((sem_t*)&_sem);
}

int Semaphore::Wait() {
  return sem_wait((sem_t*)&_sem);
}

int Semaphore::Post() {
  return sem_post((sem_t*)&_sem);
}

}  // namespace bl

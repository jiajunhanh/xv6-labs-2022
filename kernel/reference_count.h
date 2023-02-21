#ifndef XV6_LABS_2022_KERNEL_REFERENCE_COUNT_H_
#define XV6_LABS_2022_KERNEL_REFERENCE_COUNT_H_

struct reference_count {
  uint cnt;
  struct spinlock lock;
};

#endif //XV6_LABS_2022_KERNEL_REFERENCE_COUNT_H_

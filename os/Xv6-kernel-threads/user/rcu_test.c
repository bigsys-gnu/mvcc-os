/* Test new rcu syscall
   rcu call has no implementation.
*/

#include "types.h"
#include "stat.h"
#include "user.h"

int
main(void)
{
  int result = 0;

  result = rcu_init(33);
  printf(1, "rcu_init %d\n", result);

  result = rcu_reader_lock();
  printf(1, "rcu_reader_lock %d\n", result);

  result = rcu_reader_unlock();
  printf(1, "rcu_reader_unlock %d\n", result);

  result = rcu_writer_lock(13579);
  printf(1, "rcu_writer_lock %d\n", result);

  result = rcu_writer_unlock(13579);
  printf(1, "rcu_writer_unlock %d\n", result);

  result = rcu_synchronize();
  printf(1, "rcu_synchronize %d\n", result);

  result = rcu_register(3131);
  printf(1, "rcu_register %d\n", result);

  result = rcu_unregister();
  printf(1, "rcu_unregister %d\n", result);

  result = rcu_free((void *)10101);
  printf(1, "rcu_free %d\n", result);

  exit();
}

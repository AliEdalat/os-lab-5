// #include "types.h"
// #include "stat.h"
// #include "user.h"
// #include "fcntl.h"
// #include "syscall.h"

// int
// main(int argc, char* argv[])
// {
//     int pid = 1;
//     // int j = 0;
//     //chmfq(getpid(),3);
//     //chpr(getpid(),1);
//     int i;
//     int z;
//     for(i = 0; i< 15; i++){
//         if(pid>0)
//             pid=fork();
//     }
//     if(pid<0)
//         printf(1,"error\n");
//     else if(pid == 0){
//         //chmfq(getpid(),3);
//         // ps();
//         for (z = 0; z < 10000; z+=1){
//             int s;
//             s = uptime();
//             while((uptime() - s) > 100){
//                 ;
//             }
//         }
//     }
//     else {
//         for(i = 0; i< 15; i++)
//             wait();
//         printf(1,"finished:D\n");
//     }
//     exit();
// }
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
  int  k, n, id;
  double x = 0,  z;

  if(argc < 2 )
    n = 1;       //default value
  else
    n = atoi ( argv[1] ); //from command line
  if ( n < 0 || n > 20 )
    n = 2;
  x = 0;
  id = 0;
  for ( k = 0; k < n; k++ ) {
    id = fork ();
    if ( id < 0 ) {
      printf(1, "%d failed in fork!\n", getpid() );
    } else if ( id > 0 ) {  //parent
      //printf(1, "Parent %d creating child  %d\n", getpid(), id );
      //wait ();
      continue;
   } else {   // child
      //printf(1, "Child %d created\n",getpid() );
      for ( z = 0; z < 8000000.0; z += 0.01 )
         x =  x + 3.14 * 89.64;   // useless calculations to consume CPU time
      break;
    }
  }
  for ( k = 0; k < n; k++ )
    wait();
  exit();
}


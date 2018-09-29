#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

void main(){
	int read_1, write_1, write_2, write_3;
	char buff1[80] = {0};
	char buff2[] = "Hello";
  char buff3[80] = {0};
	
	read_1 = open("a.txt", O_RDONLY);
	write_1 = open("b.txt", O_CREAT|O_RDWR,S_IRWXU);
	write_2 = open("c.txt", O_CREAT|O_RDWR,S_IRWXU);
  write_3 = open("d.txt", O_CREAT|O_RDWR,S_IRWXU);

	//printf("buff1:%p\n", buff1);
	//printf("buff2:%p\n", buff2);

	read(read_1, buff1, 80);
  strcpy(buff3,buff1);	

  write(write_1, buff1, strlen(buff1));
  write(write_2, buff2, strlen(buff2));
  write(write_3, buff3, strlen(buff3));

	close(read_1);
	close(write_1);
	close(write_2);
  close(write_3);

	//printf("Done\n");
}

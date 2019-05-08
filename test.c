#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>
#include "qthread.h"
#include <netinet/in.h>     
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <time.h>

void check(int condition, char *msg);
void test1(void *ignored);
void test2(void);
void test3(void);
void test4(void);
void test5(void);
void test6(void);
void test7(void);
void test8(void);
void test9(void);
void test10(void);
void test11(void);
void test12(void);
void test13(void);
void test14(void);
void test15(void);
void test16(void);
void test17(void);
void* test_accept();

static double get_time(void){
	struct timeval tv;
	gettimeofday(&tv,NULL);
	return tv.tv_sec + tv.tv_usec/1.0e6;
}
int run_tests_ran;
void *run_tests(void *ignored)
{
	printf("----------CREATE/YIELD/JOIN TESTS----------\n");
	test1(ignored);
	test2();
	test3();
	test4();
	test5();
	
	printf("---------- MUTEX TESTS ----------\n");
	test6();
	test7();
	test8();
	test9();
	
	printf("---------- CONDITION VARIABLE TEST ----------\n");
	test10();
	test11();
	test12();
	
	printf("---------- SLEEP TEST ----------\n");
	test13();
	test14();
	test15();
	
	printf("---------- I/O TEST ----------\n");
	test16();
	test17();
	test_accept();
	
    run_tests_ran = 1;
    qthread_exit(NULL);
}
void check(int condition, char *msg)
{
    if (condition)
        return;
    printf("FAIL: %s\n", msg);
    exit(1);
}

int test1_ran;
void *test1_runner(void *ignored)
{
    test1_ran = 1;
    return (void*)1;  /* see note below */
}

void test1(void *ignored)
{
    qthread_t t = qthread_create(test1_runner, 0);
    while (!test1_ran)
        qthread_yield();
    void *v = qthread_join(t);
    check(v == (void*)1, "TEST1: v != 1");
    printf("TEST 1 PASS\n");
}
int test2_ran;
void *test2_runner(void *ignored)
{
    test2_ran = 1;
    return (void*)2;
}
void test2(void)
{
    qthread_t t = qthread_create(test2_runner, 0);
    check(test2_ran == 0, "TEST2: test2_ran != 0");
    void *v = qthread_join(t);
    check(test2_ran == 1, "TEST2: test2_ran != 1");
    check(v == (void*)2, "TEST2: v != 2");
    printf("TEST 2 PASS\n");
}
void *test3_runner(void *ignored)
{
    qthread_exit((void*)3);
}
void test3(void)
{
    qthread_t t = qthread_create(test3_runner, 0);
    void *v = qthread_join(t);
    check(v == (void*)3, "TEST 3: v != 3");
    printf("TEST 3 PASS\n");
}
int test4_ran[3];
void *test4_runner(void *val)
{
    int i = (long) val;
    check(i >= 0 && i < 3, "TEST4: bad 'i'");
    test4_ran[i] = 1;
    return val;
}

void test4(void)
{
    qthread_t t1 = qthread_create(test4_runner, (void*) 0);
    qthread_t t2 = qthread_create(test4_runner, (void*) 1);
    qthread_t t3 = qthread_create(test4_runner, (void*) 2);
    long v3 = (long) qthread_join(t3);
    long v2 = (long) qthread_join(t2);
    long v1 = (long) qthread_join(t1);

    check(test4_ran[0] && test4_ran[1] && test4_ran[2], "TEST4: test4_ran[]");
    check(v3 == 2 && v2 == 1 && v1 == 0, "TEST4: v2/1/0");
    printf("TEST 4 PASS\n");
}

int test5_ran[5];
void *test5_runner(void *val)
{
    int i = (long) val;
    check(i >= 0 && i < 5, "TEST5: bad 'i'");
    test5_ran[i] = 1;
    return val;
}

void test5(void)
{
    qthread_t t1 = qthread_create(test5_runner, (void*) 0);
    qthread_t t2 = qthread_create(test5_runner, (void*) 1);
    long v2 = (long) qthread_join(t2); //v2 = 1
    qthread_t t3 = qthread_create(test5_runner, (void*) 2);
    qthread_t t4 = qthread_create(test5_runner, (void*) 3);
    
    qthread_yield();
    qthread_t t5 = qthread_create(test5_runner, (void*) 4);
    
    long v3 = (long)qthread_join(t3); // v3 = 2
    long v5 = (long)qthread_join(t5); // v5 = 4
    long v4 = (long)qthread_join(t4); // v4 = 3
    long v1 = (long)qthread_join(t1); // v1 = 0
    

    check(test5_ran[0] && test5_ran[1] && test5_ran[2]&& test5_ran[3] && test5_ran[4], "TEST4: test4_ran[]");
    check(v1 ==  0&& v2 == 1 && v3 == 2 && v4 == 3 && v5 == 4, "TEST4: v0/1/2/3/4");
    printf("TEST 5 PASS\n");
}

// MUTEX TESTS

int test6_ran;
qthread_mutex_t m1;
void *test6_runner(void *ignored)
{
	qthread_mutex_lock(&m1);
    test6_ran = 1;
    return (void*)1;  /* see note below */
}

void test6(void)
{
	qthread_mutex_init(&m1);
    qthread_t t = qthread_create(test6_runner, 0);
    while (!test6_ran)
        qthread_yield();
    void *v = qthread_join(t);
    check(v == (void*)1, "TEST6: v != 1");
    qthread_mutex_destroy(&m1);
    printf("TEST 6 PASS\n");
}

int test7_ran;
qthread_mutex_t m2 ;
void *test7_runner(void *val)
{
	int* ret = (int*)val;
	qthread_mutex_lock(&m2);
    test7_ran = 1;
    qthread_mutex_unlock(&m2);
    return ret;  
    
}

void test7(void)
{
	
	qthread_mutex_init(&m2);
    qthread_t t1 = qthread_create(test7_runner, (void*)1);
    qthread_t t2 = qthread_create(test7_runner, (void*)2);
    
    long v1 = (long)qthread_join(t1);
    long v2 = (long)qthread_join(t2);
    
    check(v1 ==  1 && v2 == 2, "TEST6: v1/2");
    qthread_mutex_destroy(&m2);

    printf("TEST 7 PASS\n");
}

int test8_ran;
qthread_mutex_t m3;
void *test8_runner(void *val)
{
	int* ret = (int*)val;
	qthread_mutex_lock(&m3);
    test8_ran = 1;
    qthread_mutex_unlock(&m3);
    return ret;  
    
}
void test8(void)
{
	qthread_mutex_init(&m3);
    qthread_t t1 = qthread_create(test8_runner, (void*)1);
    qthread_mutex_lock(&m3);
    qthread_yield();
    qthread_yield();
    qthread_yield();
    check (test8_ran == 0, "test8_ran == 0");
    qthread_mutex_unlock(&m3);
    long v1 = (long) qthread_join(t1);
    
    check(v1 == 1 && test8_ran == 1, "v1=1, test8_ran=1");
    qthread_mutex_destroy(&m3);
    
    printf("TEST 8 PASS\n");
}

int test9_ran[3];
qthread_mutex_t m4;
void *test9_runner(void *val)
{
	int i = (long)val;
	qthread_mutex_lock(&m4);
    test9_ran[i] = 1;
    qthread_mutex_unlock(&m4);
    return val;  
    
}
void test9(void)
{
	qthread_mutex_init(&m4);
    qthread_t t1 = qthread_create(test9_runner, (void*)0);
    qthread_t t2 = qthread_create(test9_runner, (void*)1);
    qthread_t t3 = qthread_create(test9_runner, (void*)2);
    qthread_mutex_lock(&m4);
    qthread_yield();
    qthread_yield();
    qthread_yield();
    
    check (test9_ran[0] == 0 && test9_ran[1] == 0 && test9_ran[2] == 0, "test9_ran == 0/0/0");
    qthread_mutex_unlock(&m4);
    qthread_yield();
    check (test9_ran[0] == 1 && test9_ran[1] == 0 && test9_ran[2] == 0, "test9_ran == 1/0/0");
	qthread_yield();
	check (test9_ran[0] == 1 && test9_ran[1] == 1 && test9_ran[2] == 0, "test9_ran == 1/1/0");
	qthread_yield();
	check (test9_ran[0] == 1 && test9_ran[1] == 1 && test9_ran[2] == 1, "test9_ran == 1/1/1");

    qthread_mutex_destroy(&m4);

    printf("TEST 9 PASS\n");
}

int test10_ranA[3];
int test10_ranB[3];
qthread_mutex_t m5;
qthread_cond_t c1;
void *test10_runner(void *val)
{
	int i = (long)val;
	qthread_mutex_lock(&m5);
    test10_ranA[i] = 1;
    qthread_cond_wait(&c1,&m5);
    test10_ranB[i] = 1;
    qthread_mutex_unlock(&m5);
    
    return val;  
    
}
void test10(void)
{
	qthread_mutex_init(&m5);
	qthread_cond_init(&c1);
    qthread_t t1 = qthread_create(test10_runner, (void*)0);
    qthread_t t2 = qthread_create(test10_runner, (void*)1);
    qthread_t t3 = qthread_create(test10_runner, (void*)2);
    
    qthread_yield();
   
    
    check (test10_ranA[0] == 1 && test10_ranA[1] == 1 && test10_ranA[2] == 1, "test10_ranA = 1/1/1");
    check (test10_ranB[0] == 0 && test10_ranB[1] == 0 && test10_ranB[2] == 0, "test10_ranB = 0/0/0");
    
    qthread_yield();
    qthread_yield();
      
    check (test10_ranA[0] == 1 && test10_ranA[1] == 1 && test10_ranA[2] == 1, "test10_ranA = 1/1/1");
    check (test10_ranB[0] == 0 && test10_ranB[1] == 0 && test10_ranB[2] == 0, "test10_ranB = 0/0/0");
      
    qthread_cond_signal(&c1);
    qthread_yield();
    check (test10_ranB[0] + test10_ranB[1] + test10_ranB[2] == 1, "sum(test10_ranB) = 1");
    
    qthread_cond_signal(&c1);
    qthread_yield();
    check (test10_ranB[0] + test10_ranB[1] + test10_ranB[2] == 2, "sum(test10_ranB) = 2");

    qthread_cond_signal(&c1);
    qthread_yield();
    check (test10_ranB[0] + test10_ranB[1] + test10_ranB[2] == 3, "sum(test10_ranB) = 3");

	qthread_cond_destroy(&c1);
	qthread_mutex_destroy(&m5);
    printf("TEST 10 PASS\n");
}

int test11_ranA[3];
int test11_ranB[3];
qthread_mutex_t m6;
qthread_cond_t c2;
void *test11_runner(void *val)
{
	int i = (long)val;
	qthread_mutex_lock(&m6);
    test11_ranA[i] = 1;
    qthread_cond_wait(&c2,&m6);
    test11_ranB[i] = 1;
    qthread_mutex_unlock(&m6);
    
    return val;  
    
}
void test11(void)
{
	qthread_mutex_init(&m6);
	qthread_cond_init(&c2);
    qthread_t t1 = qthread_create(test11_runner, (void*)0);
    qthread_t t2 = qthread_create(test11_runner, (void*)1);
    qthread_t t3 = qthread_create(test11_runner, (void*)2);
    
    qthread_yield();
   
    
    check (test11_ranA[0] == 1 && test11_ranA[1] == 1 && test11_ranA[2] == 1, "test11_ranA = 1/1/1");
    check (test11_ranB[0] == 0 && test11_ranB[1] == 0 && test11_ranB[2] == 0, "test11_ranB = 0/0/0");
    
   
    qthread_cond_broadcast(&c2);
    qthread_yield();
    check (test11_ranB[0] + test11_ranB[1] + test11_ranB[2] == 3, "sum(test11_ranB) = 3");
    
	qthread_cond_destroy(&c2);
	qthread_mutex_destroy(&m6);
    printf("TEST 11 PASS\n");
}

int test12_ranA;
int test12_ranB;
int test12_ranC;
qthread_mutex_t m7;
qthread_cond_t c3;
void *test12_runner()
{
	qthread_mutex_lock(&m7);
    test12_ranA = 1;
    qthread_cond_wait(&c3,&m7);
    test12_ranB = 1;
    qthread_cond_wait(&c3, &m7);
    test12_ranC = 1;
    qthread_mutex_unlock(&m7);
    
    return 0;
    
}
void test12(void)
{
	qthread_mutex_init(&m7);
	qthread_cond_init(&c3);
	
    qthread_t t1 = qthread_create(test12_runner, (void*)0);
   
    qthread_yield();
    qthread_yield();
   
    check (test12_ranA == 1 && test12_ranB == 0 && test12_ranC == 0, "A/B/C = 1/0/0");
	qthread_cond_signal(&c3);
	qthread_yield();
	check (test12_ranA == 1 && test12_ranB == 1 && test12_ranC == 0, "A/B/C = 1/1/0");
	
	qthread_yield();
	qthread_yield();
	qthread_yield();
	check (test12_ranA == 1 && test12_ranB == 1 && test12_ranC == 0, "A/B/C = 1/1/0");
	
	qthread_cond_signal(&c3);
	qthread_yield();
	check (test12_ranA == 1 && test12_ranB == 1 && test12_ranC == 1, "A/B/C = 1/1/1");
	
	qthread_cond_destroy(&c3);
	qthread_mutex_destroy(&m7);
    printf("TEST 12 PASS\n");
}

void *test13_runner(){
	qthread_usleep(0.5*1.0e6);
	return 0;
}

void test13(){
	double t1 = get_time();
	qthread_t t = qthread_create(test13_runner, (void*)0);
	qthread_join(t);
	double t2 = get_time();
		
	check(0.45 < t2-t1 && t2-t1 <0.55, "Test 13 failed\n");
	printf("Test 13 PASS\n");
}

void *test14_runner(void *val){
	int i = (long)val;
	qthread_usleep(i*1.0e3);
	return 0;
}

void test14(){
	double start = get_time();
	qthread_t t1 = qthread_create(test14_runner, (void*)500);
	qthread_t t2 = qthread_create(test14_runner, (void*)500);
	qthread_t t3 = qthread_create(test14_runner, (void*)250);
	qthread_t t4 = qthread_create(test14_runner, (void*)250);
	qthread_join(t3);
	qthread_join(t4);
	double mid_time = get_time();
	check(0.2 < mid_time-start && mid_time-start <0.3, "Test 14 failed\n");

	qthread_join(t1);
	qthread_join(t2);
	double end = get_time();
		
	check(0.45 < end-start && end-start <0.55, "Test 14 failed\n");
	printf("Test 14 PASS\n");
}

void *test15_runner(void *val){
	int i = (long)val;
	qthread_usleep(i*1.0e3);
	return 0;
}

void test15(){
	double start = get_time();
	qthread_t t1 = qthread_create(test15_runner, (void*)500);
	qthread_t t2 = qthread_create(test15_runner, (void*)500);
	qthread_t t3 = qthread_create(test15_runner, (void*)250);
	qthread_t t4 = qthread_create(test15_runner, (void*)250);
	qthread_join(t1);
	qthread_join(t2);
	double mid_time = get_time();
	check(0.45 < mid_time-start && mid_time-start <0.55, "Test 15 failed\n");

	qthread_join(t3);
	qthread_join(t4);
	double end_time = get_time();
	check(0.45 < end_time-start && end_time-start <0.55, "Test 15 failed\n");

	printf("Test 15 PASS\n");
}

int fd[2];
int test16_passed = 0;
int entering_read = 0;
void* test16_child(void *v){
	char buf[100];
	entering_read = 1;
	int n = qthread_read(fd[0], buf, 100);
	check(n == 100, "Should read 100 bytes");
	char test_buf[100];
	memset(&test_buf, 'X', 100);
	
	check(memcmp(buf, test_buf, 100) == 0, "Read contents are incorrect");
	test16_passed = 1;
}
void test16(){
	pipe(fd);
	
	qthread_t t = qthread_create(test16_child, (void*)0);
	qthread_usleep(0.1*1.0e6);
	
	check(entering_read == 1 && test16_passed == 0, "Test 16 failed");
	
	char buf[100];
	memset(&buf, 'X', 100);
	write(fd[1], buf, 100);
	
	qthread_usleep(0.1*1.0e6);
	
	check(test16_passed == 1, "Test 16 failed");
	
	printf("Test 16 PASS\n");
}

int fd1[2];
int test17_passed = 0;
int entering_write = 0;
char buf[68*1024];
char buf2[68*1024];
void* test17_child(void *v){
	
	int val1 = 0, val2 = 0, len = 68*1024;
	//char *buf = malloc(len);
	memset(buf, 'X', len);
	
	entering_write = 1;
	val1 = qthread_write(fd1[1], buf, len);
	if (val1<len){
		val2 = qthread_write(fd1[1], buf, len-val1);
	}
	
	check (val1+val2 == len, "Should write 68kb");
	
	test17_passed =1;
}

void test17(){
	pipe(fd1);
	
	qthread_t t = qthread_create(test17_child, (void*)0);
	qthread_usleep(0.1*1.0e6);
	
	check(entering_write == 1 && test17_passed == 0, "Test 17 failed");
	
	//char *buf = malloc(4*1024);
	read(fd1[0], buf2, 4*1024);
	qthread_usleep(0.1*1.0e6);
	
	check(test17_passed == 1, "Test 17 failed");
	printf("Test 17 PASS\n");
	
}


int accept_flag;
int bind_port;
void *accept_runner(void *arg)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr, client_addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(bind_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int bindval = bind(s, (struct sockaddr *)&addr, sizeof(addr));
    check(bindval == 0, "Accept test failed");

    int listenval = listen(s, 5);
    check(listenval == 0, "Accept test failed");

    accept_flag = 1;
    socklen_t addr_len = sizeof(client_addr);
    int s1 = qthread_accept(s, (struct sockaddr*)&client_addr, &addr_len);
    accept_flag = 2;

    close(s1);
    close(s);
    return NULL;
}

void *test_accept(void)
{
    accept_flag = 0;
    bind_port = 5000 + (time(NULL) % 10000);

    qthread_t th = qthread_create(accept_runner, NULL);
    qthread_usleep(100000);
    check(accept_flag == 1, "Accept test failed");
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(bind_port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int connectval = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    check(connectval == 0, "Accept test failed");

    qthread_usleep(100000);
    check(accept_flag == 2, "Accept test failed");

    qthread_join(th);
    
    printf("ACCEPT TEST PASS\n");

    return NULL;
}

int main(int argc, char** argv)
{
    qthread_t t = qthread_create(run_tests, 0);
    qthread_run();
    check(run_tests_ran == 1, "MAIN: run_tests did not run\n");
    printf("DONE\n");
    return 0;
}

/* Tests unitaires pour my-sem
Copyright (C) 2016 Dubray Alexandre

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <pthread.h>
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "student_code.h"

int testFree = 0;
int testUnlock = 0;
int nb_times_malloc_used = 0;
int let_malloc_fail = 0;
int nb_times_free_used = 0;
int nb_times_unlock_used = 0;
int init_suite1(void){
	return 0;
}

int clean_suite1(void){
	return 0;
}

/* Méthode auxilaire permettant de calculer la taille d'une liste chaîneé */
int length_list(sem_process_t *head){
	int length;
	for(length = 0; head != NULL; length ++)
		head = head->next;
	return length;
}

/* Permet de compter le nombre d'appel réussis à sem_wait */
int countAppelReussis;

void *auxiWaitBloquant(void *arg){
	mysem_t * sem = (mysem_t *) arg;
	int i;
	for(i=0;i<3;i++){
		int err =mysem_wait(sem);
		if(err == 0){
			countAppelReussis ++;
		}
	}
	pthread_exit(NULL);
}

// @mysem_wait:test_wait_bloquant => [mysem_wait ne bloque pas le processus quand sem->value = 0]
void test_wait_bloquant(void){
	mysem_t *sem=(mysem_t *)malloc(sizeof(mysem_t));
	mysem_init(sem,2);
	countAppelReussis = 0;
	pthread_t th;
	pthread_create(&th,NULL,&auxiWaitBloquant,(void *)sem);
	usleep(200);
	/* On test le nombre d'appel réussis */
	CU_ASSERT_EQUAL(countAppelReussis,2);
	mysem_post(sem);
	usleep(200);
	CU_ASSERT_EQUAL(countAppelReussis,3);
	free(sem);
}
/* Threads pour le test test_wait_add_end */
void *auxi_test_wait_end1(void *arg){
	mysem_t *sem = (mysem_t *)arg;
	mysem_wait(sem);
	mysem_wait(sem);
	mysem_wait(sem);
	pthread_exit(NULL);
}

void *auxi_test_wait_end2(void *arg){
	mysem_t *sem = (mysem_t *)arg;
	mysem_wait(sem);
	pthread_exit(NULL);
}
// @mysem_wait:test_wait_add_end => [mysem_wait n'ajoute pas correctement les processus]
void test_wait_add_end(void){
	mysem_t *sem=(mysem_t *)malloc(sizeof(mysem_t));
	mysem_init(sem,2);
	pthread_t th[2];
	/* On crée le premier thread qui va mettre la sémaphore à 0 et ajouter un processus dans la queue */
	pthread_create(&(th[0]),NULL,&auxi_test_wait_end1,(void *)sem);
	usleep(200);
	sem_process_t *firstProcess = sem->head;
	if(firstProcess == NULL){
		CU_ASSERT_TRUE(0);
		free(sem);
		return;
	}
	/* On crée le deuxième thread qui va être directement bloquer et ajouter un processus dans la queue */
	pthread_create(&(th[1]),NULL,&auxi_test_wait_end2,(void *)sem);
	usleep(200);
	/* On check que la tête de la queue est toujours le premier processus */
	CU_ASSERT_PTR_EQUAL(firstProcess,sem->head);
	/* On check qu'il y a bien un deuxième élément dans la queue */
	CU_ASSERT_PTR_NOT_NULL(firstProcess->next);
	free(sem);
}

jmp_buf label_test_mysem_wait_malloc;
void sig_handler(int signo){
	longjmp(label_test_mysem_wait_malloc,1);
}

/* Threads qui va ajouter un élément dans la liste chaînée et voir si la segfault est gérée.
 * Ensuite, cette fonction fera le longjmp vers la fonction qui l'a lancée */
void *auxi_test_wait_malloc(void *arg){
	mysem_t *sem = (mysem_t *)arg;
	mysem_wait(sem);
	pthread_exit(NULL);
}

// @mysem_wait:test_wait_malloc => [mysem_wait ne gère pas le cas où malloc échoue]
void test_wait_malloc(void){
	mysem_t *sem = (mysem_t *)malloc(sizeof(mysem_t));
	if(sem == NULL){
		CU_FAIL("La mémoire n'a pas pu être allouée pour le test test_wait_malloc.");
		return;
	}
	mysem_init(sem,2);
	/* Mise de la sémaphore à 0*/
	mysem_wait(sem);
	mysem_wait(sem);
	/* test si le cas où malloc retourne null est gérer */
	let_malloc_fail = 1;
	if (signal(SIGSEGV,sig_handler) == SIG_ERR){
		CU_FAIL("Impossible d'enregistrer un signal handler.");
		return;
	}
	pthread_t th;
	if(setjmp(label_test_mysem_wait_malloc)==0){
		pthread_create(&th,NULL,&auxi_test_wait_malloc,(void *)sem);
		usleep(200);
	}
	else{
		CU_ASSERT_TRUE(0);
	}
	let_malloc_fail =0;
	signal(SIGSEGV, SIG_DFL);
	free(sem);
}

jmp_buf label_test_mysem_wait_segfault;
void sig_handler1(int signo){
	longjmp(label_test_mysem_wait_segfault,1);
}
/* Fonction qui va créer la segfault*/
void *auxi_test_wait_segfault(void *arg){
	mysem_t *sem = (mysem_t *)arg;
	mysem_wait(sem);
	pthread_exit(NULL);
}
//@mysem_wait:test_wait_segfault => [La fonction wait essaie d'accéder à des processus dans la liste alors qu'il n'y en a pas]
void test_wait_segfault(void){
	mysem_t *sem = (mysem_t *)malloc(sizeof(mysem_t));
	if(sem == NULL){
		CU_FAIL("La mémoire n'a pas pu être allouée pour le test.");
		return;
	}
	mysem_init(sem,1);
	mysem_wait(sem);
	sem->head = NULL;
	if(signal(SIGSEGV,sig_handler1)==SIG_ERR){
		CU_FAIL("Impossible d'enregistrer un signal handler.");
		return;
	}
	pthread_t th;
	if(setjmp(label_test_mysem_wait_segfault) == 0){
		pthread_create(&th,NULL,&auxi_test_wait_segfault,(void *)sem);
		usleep(200);
	}
	else{
		CU_ASSERT_TRUE(0);
	}
	signal(SIGSEGV, SIG_DFL);
	free(sem);
}

void *auxi_test_post_take_elem(void *arg){
	mysem_t *sem = (mysem_t *)arg;
	mysem_wait(sem);
	pthread_exit(NULL);
}
// @mysem_post:test_post_take_elem => [mysem_post ne retire pas de processus dans la liste]
void test_post_take_elem(void){
	mysem_t *sem=(mysem_t *)malloc(sizeof(mysem_t));
	mysem_init(sem,1);
	mysem_wait(sem);
	pthread_t th[3];
	int i;
	for(i=0;i<3;i++)
		pthread_create(&(th[i]),NULL,&auxi_test_post_take_elem,(void *)sem);
	usleep(200);
	mysem_post(sem);
	CU_ASSERT_EQUAL(length_list(sem->head),2);
	free(sem);
}

// @mysem_post:test_post_take_first => [mysem_post ne retire pas le premier processus de la liste]
void test_post_take_first(void){
	mysem_t *sem=(mysem_t *)malloc(sizeof(mysem_t));
	mysem_init(sem,1);
	mysem_wait(sem);
	pthread_t th[3];
	/* On réutilise la fonction auxiliaire du test précédent */
	int i;
	for(i=0;i<3;i++)
		pthread_create(&(th[i]),NULL,&auxi_test_post_take_elem,(void *)sem);
	usleep(200);
	sem_process_t *firstHead = sem->head;
	mysem_post(sem);
	if(firstHead == NULL){
		CU_ASSERT_TRUE(0);
		free(sem);
		return;
	}
	CU_ASSERT_PTR_EQUAL(sem->head,firstHead->next);
	free(sem);
}

// @mysem_post:test_post_exceed_capacity => [mysem_post : value > capacity]
void test_post_exceed_capacity(void){
	mysem_t *sem=(mysem_t *)malloc(sizeof(mysem_t));
	mysem_init(sem,2);
	mysem_post(sem);
	CU_ASSERT_EQUAL(sem->value,2);
	mysem_wait(sem);
	mysem_post(sem);
	mysem_post(sem);
	CU_ASSERT_EQUAL(sem->value,2);
	free(sem);
}

// @mysem_init:test_init_value => [mysem_t n'est pas bien initialisé]
void test_init_value(void){
	mysem_t *sem=(mysem_t *)malloc(sizeof(mysem_t));
	mysem_init(sem,3);
	CU_ASSERT_EQUAL(sem->value,3);
	CU_ASSERT_EQUAL(sem->value,3);
	CU_ASSERT_PTR_NULL(sem->head);
	free(sem);
}

// @mysem_wait:test_count_malloc => [La fonction mysem_wait n'appel pas le bon nombre de fois malloc]
void test_count_malloc(void){
	mysem_t *sem=(mysem_t *)malloc(sizeof(mysem_t));;
	mysem_init(sem,1);
	mysem_wait(sem);
	pthread_t th[3];
	int i;
	nb_times_malloc_used = 0;
	for(i=0;i<3;i++)
		pthread_create(&(th[i]),NULL,&auxi_test_post_take_elem,(void *)sem);
	usleep(200);
	CU_ASSERT_EQUAL(nb_times_malloc_used,3);
	free(sem);
}

/* Auxi */
void *auxi_free(void *arg){
	mysem_t *sem = (mysem_t *)arg;
	mysem_wait(sem);
	pthread_exit(NULL);
}
// @mysem_close:test_close_free => [La fonction mysem_close ne libère pas tout les blocs]
void test_close_free(void){
	mysem_t *sem = (mysem_t *)malloc(sizeof(mysem_t));
	mysem_init(sem,0);
	pthread_t th[3];
	int i;
	for(i=0;i<3;i++)
		pthread_create(&(th[i]),NULL,&auxi_free,(void *)sem);
	usleep(200);
	nb_times_free_used = 0;
	mysem_close(sem);
	CU_ASSERT_EQUAL(nb_times_free_used,4); // 4 car 3 process + la sémaphore
}

/* Fonction qui ajoute un élément à la queue */
void *auxi_test_close_unlock(void *arg){
	mysem_t *sem = (mysem_t *)arg;
	mysem_wait(sem);
	pthread_exit(NULL);
}

// @mysem_close:test_close_unlock => [La fonction d'unlock pas tous les mutex]
void test_close_unlock(void){
	mysem_t *sem = malloc(sizeof(mysem_t));
	mysem_init(sem,1);
	mysem_wait(sem);
	pthread_t th[3];
	int i;
	for(i=0;i<3;i++)
		pthread_create(&(th[i]),NULL,&auxi_test_close_unlock,(void *)sem);
	usleep(200);
	nb_times_unlock_used = 0;
	mysem_close(sem);
	CU_ASSERT_EQUAL(nb_times_unlock_used,3);
	
}
int main(){
	CU_pSuite pSuite = NULL;

	if(CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();

	pSuite = CU_add_suite("Suite de tests pour my-sem",init_suite1,clean_suite1);
	if(pSuite == NULL){
		CU_cleanup_registry();
		return CU_get_error();
	}
	if(CU_add_test(pSuite, "test_wait_bloquant",test_wait_bloquant) == NULL ||
			CU_add_test(pSuite,"test_wait_add_end",test_wait_add_end) == NULL ||
			CU_add_test(pSuite,"test_wait_malloc",test_wait_malloc) == NULL||
			CU_add_test(pSuite,"test_wait_segfault",test_wait_segfault) == NULL||
			CU_add_test(pSuite,"test_post_take_elem",test_post_take_elem) == NULL||
			CU_add_test(pSuite,"test_post_take_first",test_post_take_first) == NULL ||
			CU_add_test(pSuite,"test_post_exceed_capacity",test_post_exceed_capacity) == NULL||
			CU_add_test(pSuite,"test_init_value",test_init_value)==NULL||
			CU_add_test(pSuite,"test_count_malloc",test_count_malloc)==NULL||
			CU_add_test(pSuite,"test_close_free",test_close_free)==NULL||
			CU_add_test(pSuite,"test_close_unlock",test_close_unlock)==NULL){
		CU_cleanup_registry();
		return CU_get_error();
	}

	CU_basic_run_tests();
	CU_basic_show_failures(CU_get_failure_list());
	CU_cleanup_registry();
	return CU_get_error();
}




#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <queue>
#include <ctime>
#include <random>
#include <getopt.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <cstring>
#include <sys/time.h>

using namespace std;

int pthread_sleep(double seconds){
    pthread_mutex_t mutex;
    pthread_cond_t conditionvar;
    if(pthread_mutex_init(&mutex,NULL)){
        return -1;
    }
    if(pthread_cond_init(&conditionvar,NULL)){
        return -1;
    }
    struct timeval tp;
    struct timespec timetoexpire;
    gettimeofday(&tp, NULL);
    long new_nsec = tp.tv_usec * 1000 + (seconds - (long)seconds) * 1e9;
    timetoexpire.tv_sec = tp.tv_sec + (long)seconds + (new_nsec / (long)1e9);
    timetoexpire.tv_nsec = new_nsec % (long)1e9;
    pthread_mutex_lock(&mutex);
    int res = pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&conditionvar);
    return res;
}


int N;
int q;
double p;
float t;
float b;
queue<int> Q;
int datax=0;
int q_num=0;
int question=0;
int speak=0;
int ans=0;
int ending=0;
int finished_speaking=1;
float total_time=0;
int breaking_news=0;
// Function declaration of all required functions
void* moderator_func(void*);
void* commentator_func(void*);

//works like a real life microphone and the one who wants to speak should lock the mic while speaking
pthread_mutex_t mic = PTHREAD_MUTEX_INITIALIZER;

//signals that the moderator asked a question, now the commentators who are waiting for this signal can start to speak
pthread_cond_t questionAsked =PTHREAD_COND_INITIALIZER;

//signals that the commentators answered the question. If there are questions left, the moderator can ask the next question
//If there is no question, the session can be terminated.
pthread_cond_t questionAnswered =PTHREAD_COND_INITIALIZER;

//signals that commentators now have the permission of the moderator and can continue to execute
pthread_cond_t you_may_speak =PTHREAD_COND_INITIALIZER;

//signals that the commentators who have been added to the queue want to speak and need the permission of the moderator to speak
pthread_cond_t answer_request =PTHREAD_COND_INITIALIZER;

//used by the moderator to signal to the breaking_func that there is breaking news
pthread_cond_t news =PTHREAD_COND_INITIALIZER;

//signals that the breaking news ended from the breaking_func to the moderator_func
pthread_cond_t finishedNews =PTHREAD_COND_INITIALIZER;

//takes the number of commentators, speak time for commentators, number of questions, probability of answer and
//the probability of the breaking news with the command line arguments

void specify_number(int argc, char *argv[], int &N, double &p, int &q, float &t, float &b) {
int opt = getopt(argc, argv, "n:q:p:t:b:");
    while (opt != -1) {
        switch (opt) {
        case 'n':
            N = atoi(optarg);       
            break;
        case 'q':
            q = atoi(optarg);
            break;
         case 'p':
            p = atof(optarg);
            break;
         case 't':
            t = atof(optarg);  
            break;
         case 'b':
            b = atof(optarg);
            break;         
        default:
           exit(EXIT_FAILURE);
        }
      opt = getopt(argc, argv, "n:q:p:t:b:");
    }
}

//
void* commentator_func(void*)
{

  int prob = rand() % 100 + 1;
  srand (time(NULL));
  while(!ending){
    pthread_mutex_lock(&mic);
    //waits for the questionAsked signal and the moderator_func asks the first question
    while(question==0){
      pthread_cond_wait(&questionAsked,&mic);
    }
    question=1;
    
    if(Q.size()<N){
        //checks whether the given probability*100 is bigger than the randomly generated prob
      if(prob<p*100){
        question=1;
        int num = rand() % N + 1;
        //adding the commentators to the queue
        Q.push(num);
        
		    cout << "[0:" << total_time <<  "] " << "Commentator #"<< num<<" generates answer,position in queue: "<< Q.size()-1 <<endl;
          //the commentators who have been added to the queue signal the answer_request and wait for the you_may_speak signal
		    pthread_cond_signal(&answer_request);
          while(speak==0){
          pthread_cond_wait(&you_may_speak,&mic);
        }
		    speak=1;
        float tspeak = (float)rand() / (float) (RAND_MAX/t);
        cout << "[0:" <<total_time<<  "] " << "Commentator #"<< num<< "'s turn to speak for " << tspeak << " second" << endl;
          //all commentators in the queue have spoken for a random amount of time,
        total_time+=tspeak;
        pthread_sleep(tspeak);
        ans=1;
          //signals the questionAnswered and starts again to wait for the questionAsked

			  pthread_cond_signal(&questionAnswered);
      	cout << "[0:" <<total_time <<  "] " << "Commentator #"<< num<< " finished speaking." << endl;
      while(question<0){
        pthread_cond_wait(&questionAsked,&mic);
        
      }
        }
      //if the probability*100 is smaller than prob, the commentator does not speak
      else{
          pthread_cond_signal(&answer_request);
          ans=1;
          finished_speaking=1;
        }
      prob = rand() % 100 + 1;
    }else{
      question=0;
        //when the queue is full, it does not add any more commentator
      if(Q.size()==N){
        finished_speaking=1;
      }
    }   
    pthread_mutex_unlock(&mic);
    
  }
  if(q_num<0){
    exit(1);
  }
}

void* moderator_func(void*)
{
  if(N==0 || q==0){
	ending=1;
	} 
  while(!ending){

    int breaking = rand() % 100 + 1;
    //cout << breaking << endl; //breaking news debugging
    
    //if the randomly generated breaking is smaller than b*100, the breaking news starts, if it is not, the commentator takes the mic
    if(breaking<b*100){
      pthread_mutex_lock(&mic);
      pthread_cond_signal(&news);
      pthread_cond_wait(&finishedNews,&mic);
      breaking = rand() % 100 + 1;
      
      pthread_mutex_unlock(&mic);
    }else {
    pthread_mutex_lock(&mic);
		if (Q.size() > 0 && Q.size()!=N) {
      speak=1;
            //sends the you_may_speak if the queue is not empty
      pthread_cond_signal(&you_may_speak);
      while(ans==0){
        pthread_cond_wait(&questionAnswered,&mic);
      }
      question=0;
      ans=1;
      pthread_mutex_unlock(&mic);
    } else if(Q.size()==N && finished_speaking){
      while(Q.size()!=0){
        Q.pop();
      }
      question=0;
      pthread_cond_broadcast(&questionAsked);
      pthread_mutex_unlock(&mic);
    } else if(Q.size()==0 && finished_speaking) {
      finished_speaking=0;
        //asks the question and decreases the number of questions
      cout << "[0:" << total_time <<  "] " << "Moderator asks question "<< q-q_num <<endl;
      q_num--;
      question=1;
        //signals that a question has been asked and waits for a response
      pthread_cond_broadcast(&questionAsked);
      pthread_cond_wait(&answer_request,&mic);
      speak=1;
        //gives permission to speak to the commentator
      pthread_cond_broadcast(&you_may_speak);
      pthread_mutex_unlock(&mic);

    }
    if(q_num==-1 && finished_speaking){
      //when the speak is finished and there is no question left, it sets ending to the 1 to break from the loop
      ending=1;
    }
    }

  }
  if(q_num==-1 || N==0 || q == 0){
    //when there is no question left or initally no question or commentator, it ends the session
    cout << "End of the session" << endl;
    exit(0);
  }
  
}

void* breaking_func(void*)
{
while(1){
  srand (time(NULL));
  pthread_mutex_lock(&mic);
  //waits for moderator to signal news
  pthread_cond_wait(&news,&mic);
    breaking_news=1;
    cout << "[0:" << total_time <<  "] " << "Breaking news!" << endl;
    pthread_sleep(5);
    total_time+=5;
    cout << "[0:" << total_time <<  "] " << "Breaking news ends" << endl;
    breaking_news=0;
    //sinals finishedNews to the moderator
    pthread_cond_broadcast(&finishedNews);
    pthread_mutex_unlock(&mic);

	}
}


int main(int argc, char *argv[])
{
    pthread_mutex_init(&mic, NULL);
    pthread_cond_init(&questionAnswered, NULL);
    pthread_cond_init(&questionAsked, NULL);
    pthread_cond_init(&you_may_speak, NULL);
    pthread_cond_init(&news, NULL);
    pthread_cond_init(&finishedNews, NULL);
    
    //defaults
    N=4;
    q=5; 
    t=3;
    b=0.05;
    p=0.75;
    
    //Gives the values taken from the command line to the variables n,p,q,t,b
    specify_number(argc, argv, N, p, q, t, b);
    q_num=q-1;
    pthread_t th[N+2];

      for (int i = 0; i < N+2; i++) {
        if (i == 0) {
            if (pthread_create(&th[i], NULL, &moderator_func, NULL) != 0) {
                perror("Failed to create thread");
            }
        } else if (i == 1) {
            if (pthread_create(&th[i], NULL, &breaking_func, NULL) != 0) {
                perror("Failed to create thread");
            }
        }
        else {
            if (pthread_create(&th[i], NULL, &commentator_func, NULL) != 0) {
                perror("Failed to create thread");
            }
        }
    }

    for (int i = 0; i < N+2; i++) {
        if (pthread_join(th[i], NULL) != 0 ) {
            perror("Failed to join thread");
        }
    }

    
    pthread_mutex_destroy(&mic);
    pthread_cond_destroy(&you_may_speak);
    pthread_cond_destroy(&questionAsked);
    pthread_cond_destroy(&questionAnswered);
    pthread_cond_destroy(&news);
    pthread_cond_destroy(&finishedNews);
    return 0;

}

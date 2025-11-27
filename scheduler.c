#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "process.h"
#include "queue.h"
#include "scheduler.h"

int num_algorithms() {
  return sizeof(algorithmsNames) / sizeof(char *);
}

int num_modalities() {
  return sizeof(modalitiesNames) / sizeof(char *);
}

size_t initFromCSVFile(char* filename, Process** procTable){
    FILE* f = fopen(filename,"r");
    
    size_t procTableSize = 10;
    
    *procTable = malloc(procTableSize * sizeof(Process));
    Process * _procTable = *procTable;

    if(f == NULL){
      perror("initFromCSVFile():::Error Opening File:::");   
      exit(1);             
    }

    char* line = NULL;
    size_t buffer_size = 0;
    size_t nprocs= 0;
    while( getline(&line,&buffer_size,f)!=-1){
        if(line != NULL){
            Process p = initProcessFromTokens(line,";");

            if (nprocs==procTableSize-1){
                procTableSize=procTableSize+procTableSize;
                _procTable=realloc(_procTable, procTableSize * sizeof(Process));
            }

            _procTable[nprocs]=p;

            nprocs++;
        }
    }
   free(line);
   fclose(f);
   return nprocs;
}

size_t getTotalCPU(Process *procTable, size_t nprocs){
    size_t total=0;
    for (int p=0; p<nprocs; p++ ){
        total += (size_t) procTable[p].burst;
    }
    return total;
}

int getCurrentBurst(Process* proc, int current_time){
    int burst = 0;
    for(int t=0; t<current_time; t++){
        if(proc->lifecycle[t] == Running){
            burst++;
        }
    }
    return burst;
}

int run_dispatcher(Process *procTable, size_t nprocs, int algorithm, int modality, int quantum){

    Process * _proclist;

    qsort(procTable,nprocs,sizeof(Process),compareArrival);

    init_queue();
    size_t duration = getTotalCPU(procTable, nprocs) +1;

    for (int i = 0; i < (int)nprocs; i++) {
    enqueue(&procTable[i]);
    }

    for (int p=0; p<nprocs; p++ ){
        procTable[p].lifecycle = malloc( duration * sizeof(int));
        for(int t=0; t<duration; t++){
            procTable[p].lifecycle[t]=-1;
        }
        procTable[p].waiting_time = 0;
        procTable[p].return_time = 0;
        procTable[p].response_time = 0;
        procTable[p].completed = false;
    }
    if(algorithm == FCFS){
        int tempo = 0;
            
        while(get_queue_size() > 0){
            for(int p =0; p < nprocs; p++){
                for(int t = tempo; t < tempo + procTable[p].burst; t++){
                procTable[p].lifecycle[t] = Running;
                procTable[p].response_time = tempo; 

                    if(procTable[p].burst-1 + tempo == t){
                    procTable[p].lifecycle[t] = Finished;
                    procTable[p].return_time = t;
                    procTable[p].completed = true;
                    tempo = t + 1;
                    dequeue();
                    }
                }
            }
        }
    }
    if(algorithm == SJF){
        
        qsort(procTable, nprocs, sizeof(Process), compareBurst);
        
        if(modality == NONPREEMPTIVE){
        int tempo = 0;
            while(get_queue_size() > 0){
                for(int p =0; p < nprocs; p++){
                    for(int t = tempo; t < tempo + procTable[p].burst; t++){
                    procTable[p].lifecycle[t] = Running;
                    procTable[p].response_time = tempo; 
                    
                    if(procTable[p].burst-1 + tempo == t){
                    procTable[p].lifecycle[t] = Finished;
                    procTable[p].return_time = t;
                    procTable[p].completed = true;
                    tempo = t + 1;
                    dequeue();
                    }
                }
                }
            }
        }else if (modality == PREEMPTIVE) {

        int t = 0;

        while (get_queue_size() > 0) {
            Process *volatileprocess = dequeue();
            volatileprocess->lifecycle[t] = Running;

            if (volatileprocess->response_time == 0) {
                volatileprocess->response_time = t;
            }

            volatileprocess->burst -= 1;

            if (volatileprocess->burst == 0) {
                volatileprocess->lifecycle[t] = Finished;
                volatileprocess->return_time = t;
                volatileprocess->completed = true;
                
            } else {
                enqueue(volatileprocess);
                size_t qsize = get_queue_size();
                if (qsize > 1) {
                    Process *list = transformQueueToList();                
                    qsort(list, qsize, sizeof(Process), compareBurst);     
                    setQueueFromList(list);                               
                    free(list);
                }
            }
            t++;
        }
    }

    }
    if (algorithm == RR) {
        int t = 0; 
        while (get_queue_size() > 0) {

            Process *current = dequeue();
            int executed = 0;  
            while (executed < quantum && current->burst > 0 && t < (int)duration) {

                current->lifecycle[t] = Running;
                if (current->response_time == 0) {
                    current->response_time = t;
                }
                current->burst -= 1;
                executed++;
                t++;
            }

            if (current->burst == 0) {
                current->lifecycle[t-1] = Finished;
                current->completed = true;
                current->return_time = t-1;
            } else {
                enqueue(current);
            }
        }
    }
    if(algorithm == PRIORITIES){
        qsort(procTable, nprocs, sizeof(Process), comparePriority);
        if(modality == NONPREEMPTIVE){
            int tempo = 0;
            while(get_queue_size() > 0){
                for(int p =0; p < nprocs; p++){
                    for(int t = tempo; t < tempo + procTable[p].burst; t++){
                    procTable[p].lifecycle[t] = Running;
                    procTable[p].response_time = tempo; 

                        if(procTable[p].burst-1 + tempo == t){
                        procTable[p].lifecycle[t] = Finished;
                        procTable[p].return_time = t;
                        procTable[p].completed = true;
                        tempo = t + 1;
                        dequeue();
                        }
                    }
                }
            }
        } else if(modality == PREEMPTIVE){

            int t = 0;

            while (get_queue_size() > 0) {            
            
                Process *current = dequeue();
                current->lifecycle[t] = Running;
                if (current->response_time == 0) {
                    current->response_time = t;
                }

                current->burst -= 1;
                if (current->burst == 0) {
                    current->lifecycle[t] = Finished;
                    current->completed = true;
                    current->return_time = t;
                } else {
                    enqueue(current);
                    size_t qsize2 = get_queue_size();
                    if (qsize2 > 1) {
                        Process *list2 = transformQueueToList();
                        qsort(list2, qsize2, sizeof(Process), comparePriority);
                        setQueueFromList(list2);
                        free(list2);
                    }
                }
                t++;
            }
        }
    }
       

    printSimulation(nprocs,procTable,duration);

    for (int p=0; p<nprocs; p++ ){
        destroyProcess(procTable[p]);
    }

    cleanQueue();
    return EXIT_SUCCESS;

}
 

void printSimulation(size_t nprocs, Process *procTable, size_t duration){

    printf("%14s","== SIMULATION ");
    for (int t=0; t<duration; t++ ){
        printf("%5s","=====");
    }
    printf("\n");

    printf ("|%4s", "name");
    for(int t=0; t<duration; t++){
        printf ("|%2d", t);
    }
    printf ("|\n");

    for (int p=0; p<nprocs; p++ ){
        Process current = procTable[p];
            printf ("|%4s", current.name);
            for(int t=0; t<duration; t++){
                printf("|%2s",  (current.lifecycle[t]==Running ? "E" : 
                        current.lifecycle[t]==Bloqued ? "B" :   
                        current.lifecycle[t]==Finished ? "F" : " "));
            }
            printf ("|\n");
        
    }


}

void printMetrics(size_t simulationCPUTime, size_t nprocs, Process *procTable ){

    printf("%-14s","== METRICS ");
    for (int t=0; t<simulationCPUTime+1; t++ ){
        printf("%5s","=====");
    }
    printf("\n");

    printf("= Duration: %ld\n", simulationCPUTime );
    printf("= Processes: %ld\n", nprocs );

    size_t baselineCPUTime = getTotalCPU(procTable, nprocs);
    double throughput = (double) nprocs / (double) simulationCPUTime;
    double cpu_usage = (double) simulationCPUTime / (double) baselineCPUTime;

    printf("= CPU (Usage): %lf\n", cpu_usage*100 );
    printf("= Throughput: %lf\n", throughput*100 );

    double averageWaitingTime = 0;
    double averageResponseTime = 0;
    double averageReturnTime = 0;
    double averageReturnTimeN = 0;

    for (int p=0; p<nprocs; p++ ){
            averageWaitingTime += procTable[p].waiting_time;
            averageResponseTime += procTable[p].response_time;
            averageReturnTime += procTable[p].return_time;
            averageReturnTimeN += procTable[p].return_time / (double) procTable[p].burst;
    }


    printf("= averageWaitingTime: %lf\n", (averageWaitingTime/(double) nprocs) );
    printf("= averageResponseTime: %lf\n", (averageResponseTime/(double) nprocs) );
    printf("= averageReturnTimeN: %lf\n", (averageReturnTimeN/(double) nprocs) );
    printf("= averageReturnTime: %lf\n", (averageReturnTime/(double) nprocs) );

}
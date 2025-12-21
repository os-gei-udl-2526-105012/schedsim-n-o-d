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

    static void computeMetricsFromLifecycle(size_t nprocs, Process *pt, size_t duration) {
    for (size_t p = 0; p < nprocs; p++) {

        int first_run = -1;
        int last_run = -1;
        int cpu_used = 0;

        for (size_t t = 0; t < duration; t++) {
            if (pt[p].lifecycle[t] == Running) {
                cpu_used++;
                if (first_run == -1) first_run = (int)t;
                last_run = (int)t;
            }
        }

        if (first_run == -1) {
            pt[p].response_time = -1;
            pt[p].return_time = -1;
            pt[p].waiting_time = -1;
            continue;
        }

        int finish_time = last_run + 1; 

        pt[p].response_time = first_run - pt[p].arrive_time;
        pt[p].return_time   = finish_time - pt[p].arrive_time;
        pt[p].waiting_time  = pt[p].return_time - cpu_used;
    }
    }


    int run_dispatcher(Process *procTable, size_t nprocs, int algorithm, int modality, int quantum){


    qsort(procTable, nprocs, sizeof(Process), compareArrival);

    init_queue();

    size_t duration = getTotalCPU(procTable, nprocs) + 1;

    for (int p = 0; p < nprocs; p++) {
        procTable[p].lifecycle = malloc(duration * sizeof(int));
        for (int t = 0; t < duration; t++) {
            procTable[p].lifecycle[t] = -1;
        }
        procTable[p].waiting_time = 0;
        procTable[p].return_time = 0;
        procTable[p].response_time = -1;
        procTable[p].completed = false;
    }

    int t = 0;
    size_t next_arrival = 0;

    /* ================= FCFS ================= */
    if (algorithm == FCFS) {

        while (next_arrival < nprocs || get_queue_size() > 0) {

            while (next_arrival < nprocs &&
                   procTable[next_arrival].arrive_time <= t) {
                enqueue(&procTable[next_arrival]);
                next_arrival++;
            }

            if (get_queue_size() == 0) {
                t++;
                continue;
            }

            Process *current = dequeue();

            if (current->response_time == -1)
                current->response_time = t;

            for (int i = 0; i < current->burst; i++) {
                current->lifecycle[t++] = Running;

                while (next_arrival < nprocs &&
                       procTable[next_arrival].arrive_time <= t) {
                    enqueue(&procTable[next_arrival]);
                    next_arrival++;
                }
            }

            current->lifecycle[t] = Finished;
            current->return_time = t;
            current->completed = true;
        }
    }

    /* ================= SJF ================= */
    if (algorithm == SJF) {

        if (modality == NONPREEMPTIVE) {

            while (next_arrival < nprocs || get_queue_size() > 0) {

                while (next_arrival < nprocs &&
                       procTable[next_arrival].arrive_time <= t) {
                    enqueue(&procTable[next_arrival]);
                    next_arrival++;
                }

                if (get_queue_size() == 0) {
                    t++;
                    continue;
                }

                Process *list = transformQueueToList();
                size_t qsize = get_queue_size();
                qsort(list, qsize, sizeof(Process), compareBurst);
                setQueueFromList(list);
                free(list);

                Process *current = dequeue();

                if (current->response_time == -1)
                    current->response_time = t;

                for (int i = 0; i < current->burst; i++) {
                    current->lifecycle[t++] = Running;

                    while (next_arrival < nprocs &&
                           procTable[next_arrival].arrive_time <= t) {
                        enqueue(&procTable[next_arrival]);
                        next_arrival++;
                    }
                }

                current->lifecycle[t] = Finished;
                current->return_time = t;
                current->completed = true;
            }

        } else { /* PREEMPTIVE */

            while (next_arrival < nprocs || get_queue_size() > 0) {

                while (next_arrival < nprocs &&
                       procTable[next_arrival].arrive_time <= t) {
                    enqueue(&procTable[next_arrival]);
                    next_arrival++;
                }

                if (get_queue_size() == 0) {
                    t++;
                    continue;
                }

                Process *list = transformQueueToList();
                size_t qsize = get_queue_size();
                qsort(list, qsize, sizeof(Process), compareBurst);
                setQueueFromList(list);
                free(list);

                Process *current = dequeue();

                current->lifecycle[t] = Running;
                if (current->response_time == -1)
                    current->response_time = t;

                current->burst--;

                if (current->burst == 0) {
                    current->lifecycle[t + 1] = Finished;
                    current->return_time = t + 1;
                    current->completed = true;
                } else {
                    enqueue(current);
                }

                t++;
            }
        }
    }

    /* ================= ROUND ROBIN ================= */
    if (algorithm == RR) {

        while (next_arrival < nprocs || get_queue_size() > 0) {

            while (next_arrival < nprocs &&
                   procTable[next_arrival].arrive_time <= t) {
                enqueue(&procTable[next_arrival]);
                next_arrival++;
            }

            if (get_queue_size() == 0) {
                t++;
                continue;
            }

            Process *current = dequeue();
            int executed = 0;

            if (current->response_time == -1)
                current->response_time = t;

            while (executed < quantum && current->burst > 0) {
                current->lifecycle[t++] = Running;
                current->burst--;
                executed++;

                while (next_arrival < nprocs &&
                       procTable[next_arrival].arrive_time <= t) {
                    enqueue(&procTable[next_arrival]);
                    next_arrival++;
                }
            }

            if (current->burst == 0) {
                current->lifecycle[t] = Finished;
                current->return_time = t;
                current->completed = true;
            } else {
                enqueue(current);
            }
        }
    }

    /* ================= PRIORITIES ================= */
    if (algorithm == PRIORITIES) {

        while (next_arrival < nprocs || get_queue_size() > 0) {

            while (next_arrival < nprocs &&
                   procTable[next_arrival].arrive_time <= t) {
                enqueue(&procTable[next_arrival]);
                next_arrival++;
            }

            if (get_queue_size() == 0) {
                t++;
                continue;
            }

            Process *list = transformQueueToList();
            size_t qsize = get_queue_size();
            qsort(list, qsize, sizeof(Process), comparePriority);
            setQueueFromList(list);
            free(list);

            Process *current = dequeue();

            current->lifecycle[t] = Running;
            if (current->response_time == -1)
                current->response_time = t;

            current->burst--;

            if (current->burst == 0) {
                current->lifecycle[t + 1] = Finished;
                current->return_time = t + 1;
                current->completed = true;
            } else {
                enqueue(current);
            }

            t++;
        }
    }



    //// ALGORITMOS PARA DOS CPU. Esta parte es para el apartado de extras, se ha intentado implementar los algoritmos con dos CPU en vez de con solo una. ///
    /*       
        if (algorithm == FCFS) {
        
            int cpu_time[2] = {0, 0};

            for (int p = 0; p < nprocs; p++) {

                Process *current = &procTable[p];
                int cpu = 0;

                if (cpu_time[0] <= cpu_time[1]) {
                    cpu = 0;
                } else {
                    cpu = 1;
                }

                int start  = cpu_time[cpu];              
                int finish = start + current->burst;   

                
                for (int t = start; t < finish && t < duration; t++) {
                    current->lifecycle[t] = Running;

                    if (current->response_time == -1) {
                        current->response_time = t;
                    }
                }

                current->lifecycle[finish] = Finished;
                current->return_time = finish;
                current->completed = true;

                cpu_time[cpu] = finish;

            }
        }

        if (algorithm == SJF) {
        
            if (modality == NONPREEMPTIVE) {

                qsort(procTable, nprocs, sizeof(Process), compareBurst);

                int cpu_time[2] = {0, 0};

                for (int p = 0; p < nprocs; p++) {

                    Process *current = &procTable[p];
                    int cpu = 0;

                    if (cpu_time[0] <= cpu_time[1]) {
                        cpu = 0;
                    } else {
                        cpu = 1;
                    }

                    int start  = cpu_time[cpu];              
                    int finish = start + current->burst;   

                    for (int t = start; t < finish && t < duration; t++) {
                        current->lifecycle[t] = Running;

                        if (current->response_time == -1) {
                            current->response_time = t;
                        }
                    }

                    if (finish < duration) {
                        current->lifecycle[finish] = Finished;
                    }

                    current->return_time = finish;
                    current->completed = true;

                    cpu_time[cpu] = finish;
                }

            } else if (modality == PREEMPTIVE) {

                int t = 0;

                while (get_queue_size() > 0) {
                    
                    Process *cpu0 = NULL;
                    Process *cpu1 = NULL;

                    if (get_queue_size() > 0) {
                        cpu0 = dequeue();
                    }

                    if (get_queue_size() > 0) {
                        cpu1 = dequeue();
                    }

                    if (cpu0 != NULL) {

                        cpu0->lifecycle[t] = Running;

                        if (cpu0->response_time == -1) {
                            cpu0->response_time = t;
                        }

                        cpu0->burst = cpu0->burst - 1;
                    }
                    
                    if (cpu1 != NULL) {

                        cpu1->lifecycle[t] = Running;

                        if (cpu1->response_time == -1) {
                            cpu1->response_time = t;
                        }

                        cpu1->burst = cpu1->burst - 1;
                    }

                    if (cpu0 != NULL) {
                        if (cpu0->burst == 0) {

                            int finish0 = t + 1;
                            cpu0->lifecycle[finish0] = Finished;
                            cpu0->return_time = finish0;
                            cpu0->completed = true;

                        } else {
                            enqueue(cpu0);
                        }
                    }

                    if (cpu1 != NULL) {
                        if (cpu1->burst == 0) {

                            int finish1 = t + 1;

                            if (finish1 < duration) {
                                cpu1->lifecycle[finish1] = Finished;
                            }

                            cpu1->return_time = finish1;
                            cpu1->completed = true;

                        } else {
                            enqueue(cpu1);
                        }
                    }

                    size_t qsize = get_queue_size();
                    if (qsize > 1) {
                        Process *list = transformQueueToList();
                        qsort(list, qsize, sizeof(Process), compareBurst);
                        setQueueFromList(list);
                        free(list);
                    }

                    t = t + 1;

                }
            }
        }

        if (algorithm == PRIORITIES) {
        
            qsort(procTable, nprocs, sizeof(Process), comparePriority);
                
            if (modality == NONPREEMPTIVE) {

                int cpu_time[2] = {0, 0};

                for (int p = 0; p < nprocs; p++) {

                    Process *current = &procTable[p];
                    int cpu = 0;

                    if (cpu_time[0] <= cpu_time[1]) {
                        cpu = 0;
                    } else {
                        cpu = 1;
                    }

                    int start  = cpu_time[cpu];              
                    int finish = start + current->burst;   

                    for (int t = start; t < finish && t < duration; t++) {
                        current->lifecycle[t] = Running;

                        if (current->response_time == -1) {
                            current->response_time = t;
                        }
                    }

                    if (finish < duration) {
                        current->lifecycle[finish] = Finished;
                    }

                    current->return_time = finish;
                    current->completed = true;

                    cpu_time[cpu] = finish;
                }

            } else if (modality == PREEMPTIVE) {

                int t = 0;

                while (get_queue_size() > 0) {            
                    
                    Process *cpu0 = NULL;
                    Process *cpu1 = NULL;

                    if (get_queue_size() > 0) {
                        cpu0 = dequeue();
                    }

                    if (get_queue_size() > 0) {
                        cpu1 = dequeue();
                    }


                    if (cpu0 != NULL) {

                        cpu0->lifecycle[t] = Running;

                        if (cpu0->response_time == -1) {
                            cpu0->response_time = t;
                        }

                        cpu0->burst = cpu0->burst - 1;
                    }

                    if (cpu1 != NULL) {

                        cpu1->lifecycle[t] = Running;

                        if (cpu1->response_time == -1) {
                            cpu1->response_time = t;
                        }

                        cpu1->burst = cpu1->burst - 1;
                    }

                    if (cpu0 != NULL) {
                        if (cpu0->burst == 0) {

                            int finish0 = t + 1;

                            if (finish0 < duration) {
                                cpu0->lifecycle[finish0] = Finished;
                            }

                            cpu0->return_time = finish0;
                            cpu0->completed = true;

                        } else {
                            enqueue(cpu0);
                        }
                    }

                    if (cpu1 != NULL) {
                        if (cpu1->burst == 0) {

                            int finish1 = t + 1;

                            if (finish1 < duration) {
                                cpu1->lifecycle[finish1] = Finished;
                            }

                            cpu1->return_time = finish1;
                            cpu1->completed = true;

                        } else {
                            enqueue(cpu1);
                        }
                    }

                    size_t qsize2 = get_queue_size();
                    if (qsize2 > 1) {
                        Process *list2 = transformQueueToList();
                        qsort(list2, qsize2, sizeof(Process), comparePriority);
                        setQueueFromList(list2);
                        free(list2);
                    }

                    t = t + 1;
                }
            
            }
        }
    */
    printSimulation(nprocs, procTable, duration);
    computeMetricsFromLifecycle(nprocs, procTable, duration);
    printMetrics(duration - 1, nprocs, procTable);

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
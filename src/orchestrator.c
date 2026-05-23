#include "../include/client.h"
#include "../include/utilities.h"

#define EXECUTING 0
#define SCHEDULED 1
#define COMPLETED 2

int nr_tasks = 0, nr_tasks_executing = 0, tasks, nr_tasks_scheduled = 0, current_task = 0, tasks_completed_file;
char* output_folder;
char tasks_completed[60];
typedef struct task{
    int pid_son;
    int id_task;
    long time;
    int status;
    int expected_time;
    char* args_prog[20][20];
    char* flag;   //u ou p
    int nr_progs;
} Task;
Task *task_array = NULL;

void sigchld_handler(int signo) {
    int status;
    pid_t pid; 
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        struct timeval end_time;
        gettimeofday(&end_time, NULL);
        for(int i = 0; i<nr_tasks; i++){
            if((task_array[i].pid_son == pid) && (task_array[i].status == EXECUTING)){
                task_array[i].time = (end_time.tv_sec * 1000) + (end_time.tv_usec / 1000) - task_array[i].time;
                task_array[i].status = COMPLETED;
                //meter essa tarefa num ficheiro de tarefas completas
                tasks_completed[60] = '\0';
                sprintf(tasks_completed, "./%s/TASKS_COMPLETED.txt", output_folder);
                tasks_completed_file = open(tasks_completed, O_RDWR | O_CREAT | O_APPEND, 0666);
                char output[300];
                strcpy(output, "");
                char progs[200];
                strcpy(progs, "");
                for(int k = 0; k<task_array[i].nr_progs; k++){
                    if(k!=0) strcat(progs, " | ");
                    strcat(progs, task_array[i].args_prog[k][0]);
                }
                char temp[250];
                strcpy(temp, "");
                sprintf(temp, "%d %s",task_array[i].id_task, progs);
                strcat(output, temp);
                if(i == 2){
                    sprintf(temp, " %ld ms",task_array[i].time);
                    strcat(output, temp);
                }
                strcat(output, "\n");
                write(tasks_completed_file, output, strlen(output));
                close(tasks_completed_file);
                for(int j = 0; j<task_array[i].nr_progs; j++){
                    for(int k = 0; k<20 && task_array[i].args_prog[j][k]!=NULL; k++){
                        free(task_array[i].args_prog[j][k]);
                    }
                }
                free(task_array[i].flag);
                char task_completed[50];
                sprintf(task_completed, "TASK %d was completed in %ld milliseconds\n", task_array[i].id_task, task_array[i].time);
                write(tasks, task_completed, strlen(task_completed));
                //rearranjar o array para deixar de ter esta tarefa completa
                for (int j = i; j < nr_tasks - 1; j++) {
                    task_array[j] = task_array[j + 1];
                }

                Task *temp2 = realloc(task_array, (nr_tasks - 1) * sizeof(Task));
                task_array = temp2;
                nr_tasks--;
                nr_tasks_executing--;
                current_task--;
                if(nr_tasks==0){
                    free(task_array);
                    task_array = NULL;
                }

                break;
            }
        }
    }
}



int main(int argc, char* argv[]){
    if(argc != 4) {               // ./orchestrator output_folder parallel-tasks sched-policy
        if(argc == 2){          //  ./orchestrator -help
            if(strcmp(argv[1],"-help") == 0){
                printf("• Executar o servidor:\n\n"
                        "\t$ ./orchestrator output_folder parallel-tasks sched-policy\n\n"
                        "\tArgumentos:\n\n"
                        "\t \t1. output_folder: pasta onde são guardados os ficheiros com o output de tarefas executadas.\n"
                        "\t \t2. parallel-tasks: número de tarefas que podem ser executadas em paralelo.\n"
                        "\t \t3. sched-policy: identificador da política de escalonamento, caso o servidor suporte várias políticas.\n");
                return 0;
            }
            else{
                printf("Segundo argumento errado\n");
                return -1;
            }
        }
        else{
            if(argc < 4)printf("Falta de argumentos\n");
            if(argc > 4) printf("Argumentos em excesso\n");
            return -1;
        }
    }

    current_task = 0;
    output_folder = argv[1];
    int parallel_tasks = atoi(argv[2]);
    char* sched_policy = argv[3];
    if(strcmp(sched_policy, "FCFS") == 0 || strcmp(sched_policy, "SJF") == 0){
        if(mkfifo("client_to_server",0666) == -1){   // ligar o pipe com nome do cliente para o servidor
            perror("Erro ao criar pipe com nome");
            return -1;
        }
        if(mkfifo("server_to_client",0666) == -1){   // ligar o pipe com nome do servidor para o cliente
            perror("Erro ao criar pipe com nome");
            return -1;
        }
        mkdir(output_folder, 0777);
        int client_to_server = open("client_to_server", O_RDONLY);
        int nr_task_received = 1;
        char tasks_file[60];
        sprintf(tasks_file, "./%s/ALL_TASKS.txt", output_folder);
        tasks = open(tasks_file, O_RDWR | O_CREAT | O_APPEND | O_TRUNC, 0666);
        signal(SIGCHLD, sigchld_handler);
        while(1){   
            int bytes_read = 0;
            char buf[1000];
            char* string = NULL;
            char* args[300];
            while((bytes_read = read(client_to_server, buf, 1000)) > 0){
                args[300] = NULL;
                char* cmd_copy = strdup_n(buf, bytes_read);
                char* cmd = cmd_copy;
                int i = 0;
                while((string = strsep(&cmd, " ")) != NULL){
                    args[i] = string;
                    i++;
                }
                args[i] = NULL;

                int server_to_client = open("server_to_client", O_WRONLY);            
                if(i == 1){   // ./client status    ou      ./client end
                    if(strcmp(args[0], "status") == 0){
                        int pid = fork();
                        if(pid == -1){
                            close(server_to_client);
                        }
                        if(pid == 0){
                            for(int i = 0; i<3; i++){
                                if(i == 0) write(server_to_client, "Executing\n", 10);
                                if(i == 1) write(server_to_client, "\nScheduled\n", 11);
                                if(i == 2){
                                    write(server_to_client, "\nCompleted\n", 11);
                                    //copiar as tarefas que estão no ficheiro de tarefas completas
                                    char buffer[1000];
                                    bytes_read = 0;
                                    tasks_completed_file = open(tasks_completed, O_RDWR | O_CREAT | O_APPEND, 0666);
                                    while((bytes_read = read(tasks_completed_file, buffer, 1000)) > 0){
                                        write(server_to_client, buffer, bytes_read);
                                    }
                                    close(tasks_completed_file);
                                    break;
                                }
                                for(int j = 0; j<nr_tasks; j++){
                                    if(task_array[j].status == i){
                                        char output[300];
                                        strcpy(output, "");
                                        char progs[200];
                                        strcpy(progs, "");
                                        for(int k = 0; k<task_array[j].nr_progs; k++){
                                            if(k!=0) strcat(progs, " | ");
                                            strcat(progs, task_array[j].args_prog[k][0]);
                                        }
                                        char temp[250];
                                        strcpy(temp, "");
                                        sprintf(temp, "%d %s",task_array[j].id_task, progs);
                                        strcat(output, temp);
                                        if(i == 2){
                                            sprintf(temp, " %ld ms",task_array[j].time);
                                            strcat(output, temp);
                                        }
                                        strcat(output, "\n");
                                        write(server_to_client, output, strlen(output));
                                    }
                                }
                            }
                            close(server_to_client);
                            _exit(0);
                        }
                        close(server_to_client);
                    }
                    else{ 
                        if(strcmp(args[0], "end") == 0){
                            //liberta a memória alocada para guardar os argumentos das tarefas no array de tarefas
                            for(int i = 0; i<nr_tasks; i++){
                                for(int j = 0; j<task_array[i].nr_progs; j++){
                                    for(int k = 0; k<20 && task_array[i].args_prog[j][k]!=NULL; k++){
                                        free(task_array[i].args_prog[j][k]);
                                    }
                                }
                                free(task_array[i].flag);
                            }
                            free(task_array);
                            free(cmd_copy);
                            close(client_to_server);
                            close(server_to_client);
                            unlink("client_to_server");
                            unlink("server_to_client");
                            return 0;
                        }
                        else{
                            write(server_to_client, "Comando inválido\n", 18);
                            close(server_to_client);
                        }
                    }
                }
                else{
                    if(i > 3){  //  ./client execute time -u "prog-a [args]"
                        if((strcmp(args[0], "execute") == 0)  &&  ((strcmp(args[2], "-u") == 0) || (strcmp(args[2], "-p") == 0))){

                            char* args_prog[20][20];   //  args_prog[número do programa][número do argumento]
                            int NR_P = 0, nr_p = 0, nr_arg = 0;
                            int expected_time = atoi(args[1]);
                            for(int j = 3; j<i; j++){
                                // Se existirem mais argumentos para o mesmo programa
                                if(strcmp(args[j], "|") == 0){ 
                                    args_prog[nr_p][nr_arg] = NULL;
                                    nr_p++; // incrementa o numero do programa atual
                                    NR_P++; // incrementa o numero de programas Total
                                    nr_arg = 0;
                                }
                                else{
                                    args_prog[nr_p][nr_arg] = strdup (args[j]); // copiam-se os argumentos para a matriz
                                    nr_arg++;
                                }
                            }
                            args_prog[nr_p][nr_arg] = NULL;
                            nr_p++; // incrementa o numero do programa atual
                            NR_P++; // incrementa o numero de programas Total
                            nr_arg = 0;

                            if(((strcmp(args[2], "-u") == 0) && NR_P > 1)  ||  ((strcmp(args[2], "-p") == 0) && NR_P < 2)){
                                write(server_to_client, "Comando inválido\n", 18);
                                close(server_to_client); 
                                for(int nr_p = 0; nr_p<NR_P; nr_p++){
                                    for(int nr_arg = 0; nr_arg<20 && args_prog[nr_p][nr_arg]!=NULL; nr_arg++){
                                        free(args_prog[nr_p][nr_arg]);
                                    }
                                }
                            }
                            else{
                                char message[20];
                                // Mensagem que vai ser recebida pelo client quando se executa uma tarefa
                                sprintf(message, "\nTASK %d Received\n\n", nr_task_received);
                                write(server_to_client, message, strlen(message));
                                close(server_to_client);

                                struct timeval start_time;
                                gettimeofday(&start_time, NULL);
                                Task new_task;
                                new_task.pid_son = 0;
                                new_task.id_task = nr_task_received;
                                new_task.nr_progs = NR_P;
                                new_task.expected_time = expected_time;
                                new_task.time = (start_time.tv_sec * 1000) + (start_time.tv_usec / 1000);  //para obter o tempo em milissegundos em que recebeu a tarefa
                                new_task.status = SCHEDULED;
                                new_task.flag = strdup(args[2]);
                                // Copia-se os argumentos do programa para a matriz
                                copy_args_prog(new_task.args_prog, args_prog, NR_P);
                                // Aumenta-se o espaço utilizado, sempre que o server receber uma nova tarefa

                                task_array = realloc(task_array, (nr_tasks+1) * sizeof(Task));

                                if(strcmp(sched_policy, "FCFS") == 0) {
                                    task_array[nr_tasks] = new_task;
                                }
                                else {  // SJF
                                    if(nr_tasks == 0){
                                        task_array[0] = new_task;
                                    }
                                    else{
                                        if(current_task == nr_tasks){
                                            task_array[nr_tasks] = new_task;
                                        }
                                        else{
                                            for(int i = current_task; i<=nr_tasks; i++){
                                                if(i == nr_tasks){
                                                    task_array[nr_tasks] = new_task;
                                                    break;
                                                }
                                                if(new_task.expected_time < task_array[i].expected_time){
                                                    Task temp = new_task;
                                                    for(int j = i; j<nr_tasks; j++){
                                                        Task temp2 = task_array[j];
                                                        task_array[j] = temp;
                                                        temp = temp2;
                                                        if(j == nr_tasks-1) task_array[j+1] = temp;
                                                    }
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                                nr_tasks_scheduled++;
                                nr_tasks++;

                                for(int nr_p = 0; nr_p<NR_P; nr_p++){
                                    for(int nr_arg = 0; nr_arg<20 && args_prog[nr_p][nr_arg]!=NULL; nr_arg++){
                                        free(args_prog[nr_p][nr_arg]);
                                    }
                                }
                                nr_task_received++;
                            }
                        }
                        else{
                            write(server_to_client, "Comando inválido\n", 18);
                            close(server_to_client); 
                        }
                    }
                    else{
                        write(server_to_client, "Comando inválido\n", 18);
                        close(server_to_client);  
                    }
                }
                free(cmd_copy);
            }
            if(nr_tasks_scheduled != 0){
                if(nr_tasks_executing < parallel_tasks){
                    char taskX_file[60];
                    sprintf(taskX_file, "./%s/TASK%d.txt", output_folder, task_array[current_task].id_task);
                    int taskX = open(taskX_file, O_RDWR | O_CREAT | O_TRUNC, 0666);
                    int pid = fork();
                    if(pid == -1){
                        int server_to_client = open("server_to_client", O_WRONLY);
                        write(server_to_client, "Erro na criação do processo-filho\n", 37);
                        close(server_to_client);
                    }

                    if(pid == 0){ //processo-filho
                        if(strcmp(task_array[current_task].flag, "-u") == 0){
                            dup2(taskX, 1);
                            dup2(taskX, STDERR_FILENO);
                            close(taskX);
                            execvp(task_array[current_task].args_prog[0][0], task_array[current_task].args_prog[0]);
                            char error[50];
                            sprintf(error, "Erro no programa '%s'", task_array[current_task].args_prog[0][0]);
                            perror(error);
                            _exit(255);
                        }
                        if(strcmp(task_array[current_task].flag, "-p") == 0){
                            int nr_progs = task_array[current_task].nr_progs;
                            int pd[nr_progs - 1][2];
                            if(pipe(pd[0]) == -1){
                                    perror("Erro na criação de pipes anónimos");
                                    _exit(255);
                            }
                            int pid_filho2 = fork();
                            if(pid_filho2 == -1) perror("Erro na criação do filho");
                            else{
                                if(pid_filho2 == 0){
                                    close(pd[0][0]);
                                    dup2(pd[0][1], 1);
                                    close(pd[0][1]);
                                    dup2(taskX, STDERR_FILENO);
                                    close(taskX);
                                    execvp(task_array[current_task].args_prog[0][0], task_array[current_task].args_prog[0]);
                                    char error[50];
                                    sprintf(error, "Erro no programa '%s'", task_array[current_task].args_prog[0][0]);
                                    perror(error);
                                    _exit(255);
                                }
                                else{
                                    int status, pid_filho3;
                                    for(int i = 1; i<nr_progs - 1; i++){
                                        if(pipe(pd[i]) == -1){
                                            perror("Erro na criação de pipes anónimos");
                                            _exit(255);
                                        }
                                        if(i == 1) waitpid(pid_filho2, &status, 0);
                                        else waitpid(pid_filho3, &status, 0);
                                        pid_filho3 = fork();
                                        if(pid_filho3 == -1) perror("Erro na criação do filho");
                                        if(pid_filho3 == 0){
                                            close(pd[i-1][1]);
                                            dup2(pd[i-1][0], 0);
                                            close(pd[i-1][0]);
                                            close(pd[i][0]);
                                            dup2(pd[i][1], 1);
                                            close(pd[i][1]);
                                            dup2(taskX,STDERR_FILENO);
                                            close(taskX);
                                            execvp(task_array[current_task].args_prog[i][0], task_array[current_task].args_prog[i]);
                                            char error[50];
                                            sprintf(error, "Erro no programa '%s'", task_array[current_task].args_prog[i][0]);
                                            perror(error);
                                            _exit(255);
                                        }
                                        close(pd[i-1][1]);
                                        close(pd[i-1][0]);
                                    }
                                    if(nr_progs == 2) waitpid(pid_filho2, &status, 0);
                                    else waitpid(pid_filho3, &status, 0);
                                    int pid_filho4 = fork();
                                    if(pid_filho4 == -1) perror("Erro na criação do filho");
                                    if(pid_filho4 == 0){
                                        close(pd[nr_progs-2][1]);
                                        dup2(pd[nr_progs-2][0], 0);
                                        close(pd[nr_progs-2][0]);
                                        dup2(taskX, 1);
                                        dup2(taskX, STDERR_FILENO);
                                        close(taskX);
                                        execvp(task_array[current_task].args_prog[nr_progs-1][0], task_array[current_task].args_prog[nr_progs-1]);
                                        char error[50];
                                        sprintf(error, "Erro no programa '%s'", task_array[current_task].args_prog[nr_progs-1][0]);
                                        perror(error);
                                        _exit(255);
                                    }
                                    close(pd[nr_progs-2][0]);
                                    close(pd[nr_progs-2][1]);
                                    int status2;
                                    waitpid(pid_filho4, &status2, 0);
                                    _exit(0);
                                }
                            }
                        }
                        _exit(0);
                    }
                    else{
                        close(taskX);
                        nr_tasks_executing++;
                        nr_tasks_scheduled--;
                        task_array[current_task].pid_son = pid;
                        task_array[current_task].status = EXECUTING;
                        current_task++;
                    }
                }
            }   
        }
        close(tasks);
    }
    printf("Argumentos inválidos\n");
    return -1;
}
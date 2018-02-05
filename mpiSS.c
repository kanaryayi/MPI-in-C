#include <stdio.h>
#include <mpi.h>
#include <string.h>

#define bufSize 100

struct filefeatures{
  int most_line_count_per_thread;
  int line_count;
};

// Get lines for the thread and encrypt it with key count and return a string that includes a part of secret message
//obtained from lines of the thread
char* encrypt(char** lines,int line_count,int key){
  int line = 0 ,i = 0, msgchar=0;
  char chr=1, *message;

  message = malloc(1);

  do{

    do{
      chr = lines[line][i];
      if(((i+1)%key==0) && i!=0){
        message=realloc(message,msgchar+2);
        message[msgchar]=lines[line][i];
        message[(msgchar++)+1] ='\0';
      }
      i++;
    }while(chr !='\0');
    
    i=0;

    free(lines[i]);
  }
  while(++line < line_count);
  free(lines);
  return message;
}

//Get number of newlines
struct filefeatures* getFileFeatures(char filename[],int thread_count){
   char *buffer, chr = 0;
   int string_size, read_size, newline = 0, i = 0;
   struct filefeatures* f; //file features to return
   FILE *file = fopen(filename, "r");
   buffer = malloc(1);

   if (file)
   {
      //count newlines
       while (fgets(buffer,bufSize, file) != NULL){
          newline++;
       }

       fclose(file);
    }
    free(buffer);

    f = malloc(sizeof(struct filefeatures));
    f->most_line_count_per_thread =newline/thread_count;
    f->line_count = newline;
    return f;
}

char* getLines(char filename[],int rank,int line_per_thread,int file_line_count,char key_character){
  FILE* fp;
  int i = 0,size = 0, k=0;
  int key_count = 0,total_key_count;      // get file information line by line
  char buf[bufSize];
  char **fileLines;

  fp = fopen(filename, "r");
  fileLines = malloc(sizeof(char*));

  // Get lines for the thread related rank of the thread
  // Get number of key character per line
  while (fgets(buf, sizeof(buf), fp) != NULL){

    // Thread 1 gets lines right after Thread 0 's lines. Thread 2 gets lines right after Thread 1 's lines.
    if(i >= rank * line_per_thread && i < (rank+1)*line_per_thread && i <file_line_count){

      buf[strlen(buf) - 1] = '\0'; // read the newline
      fileLines[size] = malloc(bufSize);

      while(k++<strlen(buf)){

        if(buf[k]==key_character){

          key_count++;
        }

      }
      k=0;

      //save line
      strcpy(fileLines[size++],buf);
    }
    i++;
  }

  fclose(fp);

  //Add all result of  numbers of key character in lines
  MPI_Allreduce(&key_count,&total_key_count,1,MPI_INT,MPI_SUM,MPI_COMM_WORLD);

  return encrypt(fileLines,size,total_key_count);

}

// Here is the root method for mpi that sets ranks and size.
int startMpi(int argc,char* argv[]){

  int line_per_thread,rank,totalP,i=0,line_count,thread_message_size,message_result_size;
  char **lines,*thread_message,*message_result;
  int *displs_messages;
  int *messagesizes;
  char *fileName= argv[1];
  char key_character = argv[2][0];

  MPI_Comm_rank(MPI_COMM_WORLD,&rank); // sets ranks
  MPI_Comm_size(MPI_COMM_WORLD,&totalP); // gets total process number

  //In root process, Get file features about number of lines as a struct
  if(rank==0){
    struct filefeatures *f = getFileFeatures(fileName,totalP);
    printf("lineperthread = %d \n",f->most_line_count_per_thread);
    line_per_thread = f->most_line_count_per_thread;
    line_count =  f->line_count;
  }

  //Broadcast the most line number per thread and line number
  MPI_Bcast(&line_per_thread,1,MPI_INT,0,MPI_COMM_WORLD);
  MPI_Bcast(&line_count,1,MPI_INT,0,MPI_COMM_WORLD);

  //Call a function that gets lines and calls encrypt function to get a part of secret message
  thread_message = getLines(fileName,rank,line_per_thread,line_count,key_character);

  //Get size of the part of secret message for each thread
  thread_message_size = strlen(thread_message);

  //Sum of all parts of secret message from each thread
  MPI_Allreduce(&thread_message_size,&message_result_size,1,MPI_INT,MPI_SUM,MPI_COMM_WORLD);

  //In root process,allocate displacements and message sizes for parts of secret message to obtain overall message from
  //using Gatherv.
  if(rank==0){
    displs_messages = malloc(totalP*sizeof(int));
    messagesizes = malloc(totalP*sizeof(int));
    message_result= malloc(message_result_size+1);
    message_result[message_result_size]= 0;
  }
  //Set parts of secret message sizes in a array
  MPI_Gather(&thread_message_size,1,MPI_INT,messagesizes,1,MPI_INT,0,MPI_COMM_WORLD);

  //Set displacements
  if(rank==0){
    displs_messages[0]=0;
    for(i=1;i<totalP;i++){
      displs_messages[i]=messagesizes[i-1]+displs_messages[i-1];
    }
  }

  //Unify each parts of secret message in root process that obtained from each processes.
  MPI_Gatherv(thread_message,thread_message_size,MPI_CHAR,message_result,messagesizes,displs_messages,MPI_CHAR,0,MPI_COMM_WORLD);

  //print the result
  if(rank==0){
    printf("The secret message is ->  %s \n",message_result);
  }

  return 0;
}
int main(int argc, char *argv[]) {

  //Make the process paralel until MPI_Finalize

  printf("inputs -> text : %s, key character : %c \n",argv[1],argv[2][0]);
  
  MPI_Init(&argc,&argv);
  
  startMpi(argc,argv);

  //Stop paralelizm
  MPI_Finalize();

  return 0;
}

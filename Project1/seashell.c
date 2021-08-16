#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>            //termios, TCSANOW, ECHO, ICANON
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>
const char * sysname = "seashell";

enum return_codes {
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};
struct command_t {
	char *name;
	bool background;
	bool auto_complete;
	int arg_count;
	char **args;
	char *redirects[3]; // in/out redirection
	struct command_t *next; // for piping
};
/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t * command)
{
	int i=0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background?"yes":"no");
	printf("\tNeeds Auto-complete: %s\n", command->auto_complete?"yes":"no");
	printf("\tRedirects:\n");
	for (i=0;i<3;i++)
		printf("\t\t%d: %s\n", i, command->redirects[i]?command->redirects[i]:"N/A");
	printf("\tArguments (%d):\n", command->arg_count);
	for (i=0;i<command->arg_count;++i)
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	if (command->next)
	{
		printf("\tPiped to:\n");
		print_command(command->next);
	}


}
/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command)
{
	if (command->arg_count)
	{
		for (int i=0; i<command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}
	for (int i=0;i<3;++i)
		if (command->redirects[i])
			free(command->redirects[i]);
	if (command->next)
	{
		free_command(command->next);
		command->next=NULL;
	}
	free(command->name);
	free(command);
	return 0;
}
/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt()
{
	char cwd[1024], hostname[1024];
    gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}
/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command)
{
	const char *splitters=" \t"; // split at whitespace
	int index, len;
	len=strlen(buf);
	while (len>0 && strchr(splitters, buf[0])!=NULL) // trim left whitespace
	{
		buf++;
		len--;
	}
	while (len>0 && strchr(splitters, buf[len-1])!=NULL)
		buf[--len]=0; // trim right whitespace

	if (len>0 && buf[len-1]=='?') // auto-complete
		command->auto_complete=true;
	if (len>0 && buf[len-1]=='&') // background
		command->background=true;

	char *pch = strtok(buf, splitters);
	command->name=(char *)malloc(strlen(pch)+1);
	if (pch==NULL)
		command->name[0]=0;
	else
		strcpy(command->name, pch);

	command->args=(char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index=0;
	char temp_buf[1024], *arg;
	while (1)
	{
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch) break;
		arg=temp_buf;
		strcpy(arg, pch);
		len=strlen(arg);

		if (len==0) continue; // empty arg, go for next
		while (len>0 && strchr(splitters, arg[0])!=NULL) // trim left whitespace
		{
			arg++;
			len--;
		}
		while (len>0 && strchr(splitters, arg[len-1])!=NULL) arg[--len]=0; // trim right whitespace
		if (len==0) continue; // empty arg, go for next

		// piping to another command
		if (strcmp(arg, "|")==0)
		{
			struct command_t *c=malloc(sizeof(struct command_t));
			int l=strlen(pch);
			pch[l]=splitters[0]; // restore strtok termination
			index=1;
			while (pch[index]==' ' || pch[index]=='\t') index++; // skip whitespaces

			parse_command(pch+index, c);
			pch[l]=0; // put back strtok termination
			command->next=c;
			continue;
		}

		// background process
		if (strcmp(arg, "&")==0)
			continue; // handled before

		// handle input redirection
		redirect_index=-1;
		if (arg[0]=='<')
			redirect_index=0;
		if (arg[0]=='>')
		{
			if (len>1 && arg[1]=='>')
			{
				redirect_index=2;
				arg++;
				len--;
			}
			else redirect_index=1;
		}
		if (redirect_index != -1)
		{
			command->redirects[redirect_index]=malloc(len);
			strcpy(command->redirects[redirect_index], arg+1);
			continue;
		}

		// normal arguments
		if (len>2 && ((arg[0]=='"' && arg[len-1]=='"')
			|| (arg[0]=='\'' && arg[len-1]=='\''))) // quote wrapped arg
		{
			arg[--len]=0;
			arg++;
		}
		command->args=(char **)realloc(command->args, sizeof(char *)*(arg_index+1));
		command->args[arg_index]=(char *)malloc(len+1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count=arg_index;
	return 0;
}
void prompt_backspace()
{
	putchar(8); // go back 1
	putchar(' '); // write empty over
	putchar(8); // go back 1 again
}
/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command)
{
	int index=0;
	char c;
	char buf[4096];
	static char oldbuf[4096];

    // tcgetattr gets the parameters of the current terminal
    // STDIN_FILENO will tell tcgetattr that it should write the settings
    // of stdin to oldt
    static struct termios backup_termios, new_termios;
    tcgetattr(STDIN_FILENO, &backup_termios);
    new_termios = backup_termios;
    // ICANON normally takes care that one line at a time will be processed
    // that means it will return if it sees a "\n" or an EOF or an EOL
    new_termios.c_lflag &= ~(ICANON | ECHO); // Also disable automatic echo. We manually echo each char.
    // Those new settings will be set to STDIN
    // TCSANOW tells tcsetattr to change attributes immediately.
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);


    //FIXME: backspace is applied before printing chars
	show_prompt();
	int multicode_state=0;
	buf[0]=0;
  	while (1)
  	{
		c=getchar();
		// printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

		if (c==9) // handle tab
		{
			buf[index++]='?'; // autocomplete
			break;
		}

		if (c==127) // handle backspace
		{
			if (index>0)
			{
				prompt_backspace();
				index--;
			}
			continue;
		}
		if (c==27 && multicode_state==0) // handle multi-code keys
		{
			multicode_state=1;
			continue;
		}
		if (c==91 && multicode_state==1)
		{
			multicode_state=2;
			continue;
		}
		if (c==65 && multicode_state==2) // up arrow
		{
			int i;
			while (index>0)
			{
				prompt_backspace();
				index--;
			}
			for (i=0;oldbuf[i];++i)
			{
				putchar(oldbuf[i]);
				buf[i]=oldbuf[i];
			}
			index=i;
			continue;
		}
		else
			multicode_state=0;

		putchar(c); // echo the character
		buf[index++]=c;
		if (index>=sizeof(buf)-1) break;
		if (c=='\n') // enter key
			break;
		if (c==4) // Ctrl+D
			return EXIT;
  	}
  	if (index>0 && buf[index-1]=='\n') // trim newline from the end
  		index--;
  	buf[index++]=0; // null terminate string

  	strcpy(oldbuf, buf);

  	parse_command(buf, command);

  	// print_command(command); // DEBUG: uncomment for debugging

    // restore the old settings
    tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
  	return SUCCESS;
}
int process_command(struct command_t *command);
int main()
{
	while (1)
	{
		struct command_t *command=malloc(sizeof(struct command_t));
		memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

		int code;
		code = prompt(command);
		if (code==EXIT) break;

		code = process_command(command);
		if (code==EXIT) break;

		free_command(command);
	}

	printf("\n");
	return 0;
}

int process_command(struct command_t *command)
{
	int r;
	char *p;
	FILE *fp;
	FILE *fp2;
	char txtFile[100];
	int result = 0;
	if (strcmp(command->name, "")==0) return SUCCESS;

	if (strcmp(command->name, "exit")==0)
		return EXIT;

	if (strcmp(command->name, "cd")==0)
	{
		if (command->arg_count > 0)
		{
			r=chdir(command->args[0]);
			if (r==-1)
				printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
			return SUCCESS;
		}
	}
	
	
	//Q2.shortdir
	if(strcmp(command->name, "shortdir") == 0) {
	//keep track of path of the associations.txt
	char *buf;
	buf=(char *)malloc(10*sizeof(char));
	buf=getlogin();
	char path2[50];
	char path[50] = "/home/";
	strcat(path, buf);
	strcat(path, "/");
	strcpy(path2, path);
	fp = fopen("associations.txt", "a");
	strcat(path, "associations.txt");
	//shortdir set
	if(strcmp(command->args[0], "set") == 0) {
		//open associations.txt to associate a new short name-current directory pair
		fp = fopen(path, "a+");
		
		char cwd[100];
		char line[256];
		char line2[256];
		int exist=0;
		if (getcwd(cwd, sizeof(cwd)) != NULL) {
		while (fgets(line, sizeof(line), fp) != NULL) {
		if ((p = strchr(line, '\n')) != NULL) {
    		  *p = '\0';
    		  }
		if(strcmp(line, cwd) == 0) {
			printf("This path already exists!\n");
			exist=1;
		}
		if(strcmp(line,command->args[1] ) == 0) {
			printf("This name already exists!\n");
			exist=1;
		}		
		}
		if(exist==0){
			//write the name to the txt file
			fprintf(fp, "%s", command->args[1]);
			fprintf(fp, "\n");
			//get the path and write it to the txt file
		        fprintf(fp, "%s", cwd);
			fprintf(fp, "\n");
			//Notify user
			printf("%s is set as an alias for %s\n", command->args[1], cwd);
		}
		fclose(fp);
		
	}
	}
	//shortdir jump	
	if(strcmp(command->args[0], "jump") == 0) {
	char line[256];
	fp = fopen(path, "r");
	//find the given short name in the associations.txt
	while (fgets(line, sizeof(line), fp) != NULL) {
		if ((p = strchr(line, '\n')) != NULL) {
    		  *p = '\0';
    		  }
		if(strcmp(command->args[1], line) == 0) {
			//when found get the directory it is associated with
			fgets(line, sizeof(line), fp);
			char *path_tokenizer = strtok(line, "\n");
  			if (path_tokenizer != NULL) {
  				//go to that directory
				result = chdir(line);
				if(result == 0){
				//Notify user
				printf("Directory changed to %s\n", line);
				} 					  
			}
		}
	}
	fclose(fp);
	}
	
	//shortdir del	
	if(strcmp(command->args[0], "del") == 0) {
	//store the path of the current directory 
	char cwd[100];
	getcwd(cwd, sizeof(cwd));
	//go to the path where associations.txt exists
	chdir(path2);
	//open a temporary txt file and transfer each line except the one to be deleted
	rename("associations.txt", "temp.txt");
	fp = fopen("associations.txt", "a");
	char line[256];
	char line2[256];
	fp2 = fopen("temp.txt", "r");
	//traverse over the txt file
	while (fgets(line, sizeof(line), fp2) != NULL) {
    		fgets(line2, sizeof(line2), fp2);
		char *path_tokenizer = strtok(line, "\n");
  		if (path_tokenizer != NULL) {
  			char *path_tokenizer2 = strtok(line2, "\n");
  			if (path_tokenizer2 != NULL) {
  				//when the association to be deleted found skip those to lines
  				if(strcmp(command->args[1], line) == 0) {
  					fgets(line, sizeof(line), fp);
  					fgets(line2, sizeof(line2), fp);
   			  	} else {
   			  	//transfer other lines
    			  		fprintf(fp, "%s\n%s\n", line, line2);
    			  	}
    			 }
    		}   			  
	}
	//remove the temporary file
	remove("temp.txt");
	fclose(fp);
	//go back to the current directory
	chdir(cwd);
	}
	
	//shortdir clear		
	if(strcmp(command->args[0], "clear") == 0) {
	//go to the path where associations.txt exists
	chdir(path2);
	//remove the txt file and open a new one with the same name
	if (remove("associations.txt") == 0) {
		fp = fopen("associations.txt", "w");
		fclose(fp);
	}
	}
	
	//shortdir list
	if(strcmp(command->args[0], "list") == 0) {
	char line[256];
	fp = fopen(path, "r");
	char line2[256];
	//Traverse over the txt file
	while (fgets(line, sizeof(line), fp) != NULL) {
		if ((p = strchr(line, '\n')) != NULL) {
    		  *p = '\0';
    		  }
    		//Print all pairs
    		fgets(line2, sizeof(line2), fp);
		char *path_tokenizer = strtok(line, "\n");
  		if (path_tokenizer != NULL) {
  		char *path_tokenizer2 = strtok(line2, "\n");
  		if (path_tokenizer2 != NULL) {
    			printf("%s is set as an alias for %s\n", line, line2);
    		}
    		}   			  
	}
	fclose(fp);
	}
	}
	
	
	//Q3.highlight
	if (strcmp(command->name, "highlight")==0){
	int i;
	char search[100];
	//makes the searched word lowercase
	for(i=0;i<=strlen(command->args[0]);i++){
		search[i]=tolower(command->args[0][i]);
	}
	char line[1024];
	char lower_line[1024];
	fp = fopen(command->args[2], "r");
	//starts to read the file to find word
	while(fgets(line, sizeof(line), fp) != NULL){
        //makes the current line lowercase and saves it into lower_line to make comparison
        	for(i=0;i<=strlen(line);i++){
        	lower_line[i]=tolower(line[i]);
    		}
		//checks whether the word exists in the current file
		if(strstr(lower_line,search)!=NULL){
			if ((p = strchr(line, '\n')) != NULL) {
				*p = '\0';
			}
		//splits the line from spaces between words
		char delim[]=" ";
		char *ptr = strtok(line, delim);
		char ptr_lower[256];
		while(ptr!=NULL){
			for(i=0;i<=strlen(ptr);i++){
			ptr_lower[i]=tolower(ptr[i]);
			}
			//when the current word of the line is same with the searched word, it checks the color
			if(strcmp(ptr_lower,search)==0){
				if(strcmp(command->args[1], "r") == 0) {
				printf("\033[1;31m");
				printf("%s",ptr);
				}
				else if(strcmp(command->args[1], "g") == 0) {
				printf("\033[0;32m");
				printf("%s",ptr);
				}
				else if(strcmp(command->args[1], "b") == 0) {
				printf("\033[0;34m");
				printf("%s",ptr);
				}
				else{
				printf("Unknown color!");
				}
				printf("\033[0m");
			}else{
			printf("%s",ptr);
			}
		ptr = strtok(NULL, delim);
		if(ptr!=NULL){
		printf(" ");
		} else {
		printf("\n");
		}
		}
		}
	}
	fclose(fp);
	}
	
	
	//Q4.goodMorning
        if(strcmp(command->name, "goodMorning")==0) {
        char *p;
        int hour;
        int min;
        char *temp = command->args[1];
         
        //store the given hour and minute in the variables hour and min
        char *ptr = strtok(command->args[0], ".");
	if (ptr != NULL)
	{
		hour = atoi(ptr);
		min = atoi(strtok(NULL, "."));
	}
	//Open a temporary txt file to store the command to be executed
	FILE *fp = fopen("temp", "w");
	//Play the given music using rhythmbox everyday
	fprintf(fp, "%d %d * * * DISPLAY=:0.0 /usr/bin/rhythmbox-client --play %s\n", min, hour, temp);
	//Stop playing after 1 minute
	fprintf(fp, "%d %d * * * pkill rhythmbox\n", min + 1, hour);
	fclose(fp);
	char *cronArgs[] = {
		"crontab",
		"temp",
		NULL};
	//execute the commands provided in the txt file temp
	int a = execlp("crontab", "crontab", "temp", NULL);
	//remove the temporary txt
	remove("temp");
	}
	
	
	//Q5.kdiff
	if(strcmp (command->name, "kdiff") == 0) {
		//kdiff -a
		if(strcmp(command->args[0], "-a")==0){
		FILE *fptr1, *fptr2;
	    	int num = 1;
	    	int diff = 0;
	    	
	    	//check extension by checking th elast 4 characters of the given files
	    	//if not ".txt" notify user
	    	char extension1 = command->args[1][strlen(command->args[1])-4];
	    	char extension2 = command->args[2][strlen(command->args[2])-4];
	    	
	  	if(strcmp(&extension1,".txt")==0) {
	  	printf("First file is not a txt file!\n");
	  	return 0;
	  	}
	  	if(strcmp(&extension2,".txt")==0) {
	  	printf("Second file is not a txt file!\n");
	  	return 0;
	  	}
	    	//check if the provided txt files exist
	    	fptr1 = fopen(command->args[1], "r");
		if(fptr1 == NULL){
		printf("Cannot open file %s\n%s\n", command->args[1], strerror(errno));
		return 0;
		}
		fptr2 = fopen(command->args[2], "r");
		if (fptr2 == NULL){
		printf("Cannot open file %s\n%s\n", command->args[2], strerror(errno));
		return 0;
		}
		
		char line[256];
		char line2[256];
		//traverse over the first file (assuming both files are of the same length)
		while (fgets(line, sizeof(line), fptr1) != NULL) {
		fgets(line2, sizeof(line2), fptr2);
    		//compare each line; if they are different notify user
    		//and increment the number of different lines
		if(strcmp(line, line2) != 0) {
			printf("%s: Line %d:%s", command->args[1], num, line);
			printf("%s: Line %d:%s", command->args[2], num, line2);
			diff++;
		}
		//increment the line number
		num++;
		}
		//Notify user about the number of different lines; if 0 notify that they are identical
		if(diff == 0) {
			printf("The two text files are identical\n");
		}else {
			printf("%d different lines found\n", diff);
		}
		}
		//kdiff -b
		else if(strcmp(command->args[0], "-b")==0){
		FILE *fptr1, *fptr2;
	    	char ch1, ch2;
	    	int diff = 0;
	    	//check if the provided txt files exist
	    	fptr1 = fopen(command->args[1], "r");
	    	fptr2 = fopen(command->args[2], "r");
		if(fptr1 == NULL){
		printf("Cannot open file %s\n%s\n", command->args[1], strerror(errno));
		return 0;
		}
		if (fptr2 == NULL){
		printf("Cannot open file %s\n%s\n", command->args[2], strerror(errno));
		return 0;
		}
		//traverse over the files char by char
		do
    		{
    		//check each char if not same increment number of different bytes (char = 1 byte)
		ch1 = fgetc(fptr1);
		ch2 = fgetc(fptr2);
        	if (ch1 != ch2) {
        		diff++;
        	}
        	}while (ch1 != EOF || ch2 != EOF);
        	
        	//Notify user about the number of different bytes; if 0 notify that they are identical
		if(diff == 0) {
			printf("The two files are identical\n");
		}else {
			printf("The two files are different in %d bytes\n", diff);
		}
		fclose(fptr1);
		fclose(fptr2);
		}
		//default assume -a
		//same code as above
		else {
		FILE *fptr1, *fptr2;
	    	int num = 1;
	    	int diff = 0;
	    	
	    	//check extension by checking th elast 4 characters of the given files
	    	//if not ".txt" notify user
	    	char extension1 = command->args[0][strlen(command->args[0])-4];
	    	char extension2 = command->args[1][strlen(command->args[1])-4];
	    	
	  	if(strcmp(&extension1,".txt")==0) {
	  	printf("First file is not a txt file!\n");
	  	return 0;
	  	}
	  	if(strcmp(&extension2,".txt")==0) {
	  	printf("Second file is not a txt file!\n");
	  	return 0;
	  	}
	    	//check if the provided txt files exist
	    	fptr1 = fopen(command->args[0], "r");
		if(fptr1 == NULL){
		printf("Cannot open file %s\n%s\n", command->args[0], strerror(errno));
		return 0;
		}
		fptr2 = fopen(command->args[1], "r");
		if (fptr2 == NULL){
		printf("Cannot open file %s\n%s\n", command->args[1], strerror(errno));
		return 0;
		}
		
		char line[256];
		char line2[256];
		//traverse over the first file (assuming both files are of the same length)
		while (fgets(line, sizeof(line), fptr1) != NULL) {
    		fgets(line2, sizeof(line2), fptr2);
    		//compare each line; if they are different notify user
    		//and increment the number of different lines
		if(strcmp(line, line2) != 0) {
			printf("%s: Line %d:%s", command->args[0], num, line);
			printf("%s: Line %d:%s", command->args[1], num, line2);
			diff++;
		}
		//increment the line number
		num++;
		}
		//Notify user about the number of different lines; if 0 notify that they are identical
		if(diff == 0) {
			printf("The two text files are identical\n");
		}else {
			printf("%d different lines found\n", diff);
		}
	}
	}
	
	//Q6.Custom command: Move
	//command->args[0] : file name
	//command->args[1]: path to be moved to
	if(strcmp(command->name, "move") == 0){
	FILE *fptr1, *fptr2;
	char c;
	char cwd[100];
	//store the current path in cwd
	getcwd(cwd, sizeof(cwd));
	//if trying to move to the same directory give an error
	if(strcmp(cwd, command->args[1])==0)
	{
	printf("Cannot move to same directory!\n");
	}
	else {
	fptr1 = fopen(command->args[0], "r");
	//check if the provided file exists
	if (fptr1 == NULL)
	{
	printf("Cannot open file %s \n", command->args[0]);
	}
	//go to the path provided (to the path where the file will be moved to)
	chdir(command->args[1]);
	//open a new file with the same name
	fptr2 = fopen(command->args[0], "r");
	if(fptr2 != NULL) {
	printf("File already exists in the given path!\n");
	return 0;
	} else {
	//Transfer char by char
	c = fgetc(fptr1);
	while (c != EOF)
	{
	fputc(c, fptr2);
	c = fgetc(fptr1);
	}
	fclose(fptr2);
	//Go back to the path where original file exists
	chdir(cwd);
	//Remove the old file
	if(strcmp(cwd, command->args[1])!=0)
	{
	remove(command->args[0]);
	}
	}
	}
	}
	
	pid_t pid=fork();
	if (pid==0) // child
	{
		/// This shows how to do exec with environ (but is not available on MacOs)
	    // extern char** environ; // environment variables
		// execvpe(command->name, command->args, environ); // exec+args+path+environ

		/// This shows how to do exec with auto-path resolve
		// add a NULL argument to the end of args, and the name to the beginning
		// as required by exec

		// increase args size by 2
		command->args=(char **)realloc(
			command->args, sizeof(char *)*(command->arg_count+=2));

		// shift everything forward by 1
		for (int i=command->arg_count-2;i>0;--i)
			command->args[i]=command->args[i-1];

		// set args[0] as a copy of name
		command->args[0]=strdup(command->name);
		// set args[arg_count-1] (last) to NULL
		command->args[command->arg_count-1]=NULL;

		//execvp(command->name, command->args); // exec+args+path
		//exit(0);
		/// TODO: do your own exec with path resolving using execv()
		//get the path and tokenize it from :
		char *path = getenv("PATH");
  		char *path_tokenizer = strtok(path, ":");
  		while (path_tokenizer != NULL) {
  		//create a string for the full path
		char full_path[strlen(path_tokenizer) + strlen(command->name) + 1];
		//get the last character of the path
		const char* last = path_tokenizer;
		while (*(last + 1) != '\0') {
		    last++;
		}
		//check if the last char is /
		int check_last = strcmp(last, "/") == 0;
		//copy the path to the string "full_path"
		strcpy(full_path, path_tokenizer);
		//if not / add it to the end of the path
		if (!check_last) {
		    strcat(full_path, "/");
		}
		//add the command name given by the user
        	strcat(full_path, command->name);
        	//execute it using execv
		execv(full_path, command->args);
		path_tokenizer = strtok(NULL, ":");
    		}
  		exit(0);
	}
	else
	{
		if (!command->background)
			wait(0); // wait for child process to finish
		return SUCCESS;
	}

	// TODO: your implementation here

	printf("-%s: %s: command not found\n", sysname, command->name);
	return UNKNOWN;
}

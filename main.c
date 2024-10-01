#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
// #include<assert.h>
#include<unistd.h>

// normal strings
#define STRING_SIZE 100
// to buffer some command outputs, might be small
#define OUTPUT_BUFFER_SIZE 1000
#define DEFAULT_GPG_RECIPIENT "sometest"
#define CONFIG_FILENAME "open.conf"

#define TRUE 1
#define FALSE 0

char CurrentErrorMessage[STRING_SIZE] = { 0 };

typedef struct {
	char key[STRING_SIZE];
	char value[STRING_SIZE];
} KeyValue;

typedef struct {
	KeyValue key_values[STRING_SIZE];
	size_t size;
} Config;

char parse_line(KeyValue * kv, char *line)
{
	char *c = line;
	char reading_value = 0;
	char *pointer = kv->key;

	while (*c != '\0') {
		if (*c == '=') {
			if (reading_value) {
				*pointer++ = *c;
			}
			*pointer = '\0';
			pointer = kv->value;
			reading_value = 1;
		} else {
			*pointer++ = *c;
		}
		c++;
	}

	if (!reading_value || strlen(kv->value) == 0) {
		printf("ERROR: missing value\n");
		return FALSE;
	}

	if (strlen(kv->key) == 0) {
		printf("ERROR: missing key\n");
		return FALSE;
	}

	*pointer = '\0';
	return TRUE;
}

int parse_config_file(Config * config, char *filepath)
{
	FILE *file = fopen(filepath, "r");
	if (file == NULL) {
		return FALSE;
	}

	config->size = 0;
	char buffer[OUTPUT_BUFFER_SIZE] = { 0 };
	char c = 0;
	char *b = buffer;

	while ((c = fgetc(file)) != EOF) {
		if (c == '\n') {
			*b = '\0';
			if (parse_line
			    (&config->key_values[config->size], buffer)) {
				config->size++;
			}
			b = buffer;
		} else {
			*b++ = c;
		}
	}

	return TRUE;
}

int run_command(char *command)
{
	printf("run_command(%s)\n", command);
	int status = system(command);
	printf("(%d) => %s\n", status, command);
	return status;
}

char run_save_stdout(char *command, char *output)
{
	FILE *fp = popen(command, "r");
	if (fp == NULL) {
		printf("Failed to run command\n");
		return 1;
	}

	fgets(output, OUTPUT_BUFFER_SIZE, fp);

	int status = pclose(fp);
	printf("(%d) => %s => %s\n", status, command, output);

	return status;
}

const char *get_filename_ext(const char *filename)
{
	const char *dot = strrchr(filename, '.');
	if (!dot || dot == filename)
		return "";
	return dot;
}

// TODO make this better
char check_url_simple(char *url)
{
	char *u = url;
	char *should_match = "http.://";
	char *s = should_match;
	while (*u != '\0' && *s != '\0') {
		if (*s != '.' && *u != *s) {
			return FALSE;
		}
		u++;
		s++;
	}
	return *s == '\0';
}

char get_filetype(char *filetype, char *filename)
{
	if (check_url_simple(filename)) {
		strcpy(filetype, "URL");
		return TRUE;
	}

	char buffer[OUTPUT_BUFFER_SIZE] = { 0 };
	sprintf(buffer, "file -b \"%s\"", filename);

	if (run_save_stdout(buffer, filetype) != 0) {
		return FALSE;
	}
	// TODO this is not an error from file? maybe detect filetype in some other way
	// TODO maybe instead of relying on `file`, just use extensions?
	if (strstr(filetype, "No such file or directory")) {
		return FALSE;
	}

	return TRUE;
}

int match_filename_with_config(Config config, char *filename)
{
	const char *current_extension = get_filename_ext(filename);
	printf("file ext: %s\n", current_extension);

	char filetype[OUTPUT_BUFFER_SIZE] = { 0 };
	if (!get_filetype(filetype, filename)) {
		printf("ERROR: failed to get filetype from %s\n", filename);
		// TODO make this error general or find a better way to do this
		strcpy(CurrentErrorMessage, "Failed to shred tmp file");
		return -1;
	}

	printf("filename=%s\nfile output=%s\n", filename, filetype);
	for (size_t i = 0; i < config.size; i++) {
		KeyValue kv = config.key_values[i];
		if (strcmp(current_extension, kv.key) == 0 ||
		    strstr(filetype, kv.key)) {
			return i;
		}
	}

	return -1;
}

char get_config_filepath(char *output)
{
	sprintf(output, "/etc/%s", CONFIG_FILENAME);
	if (access(output, F_OK) == 0) {
		return TRUE;
	}

	char *path_suffixs[3] = {
		".config",
		"Programming/c/open",
	};

	for (int i = 0; i < 3; i++) {
		// TODO it returns something, check that
		sprintf(output, "%s/%s/%s", getenv("HOME"), path_suffixs[i],
			CONFIG_FILENAME);
		if (access(output, F_OK) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

int mktemp_file(char *output_buffer)
{
	if (run_save_stdout("mktemp", output_buffer) != 0)
		return FALSE;
	char *c = output_buffer;
	while (*c != '\n')
		c++;
	*c = '\0';

	return TRUE;
}

char md5_hash_filepath(char *output_buffer, char *filepath)
{
	// TODO filepath might be bigger?
	char cmd_buffer[STRING_SIZE * 2] = { 0 };
	sprintf(cmd_buffer, "md5sum %s", filepath);
	if (run_save_stdout(cmd_buffer, output_buffer) != 0)
		return FALSE;

	char *c = output_buffer;
	while (*c != ' ')
		c++;
	*c = '\0';

	return TRUE;
}

char format_command_open_file(char *final_command, Config config,
			      char *filepath)
{
	int config_index = match_filename_with_config(config, filepath);
	if (config_index == -1) {
		return FALSE;
	}

	printf("Matched config: key=%s value=%s\n",
	       config.key_values[config_index].key,
	       config.key_values[config_index].value);
	sprintf(final_command, "%s %s", config.key_values[config_index].value,
		filepath);

	return TRUE;
}

char decrypt_reencrypt_pgp(Config config, char *filepath)
{
	char tmp_filepath[STRING_SIZE] = { 0 };
	if (!mktemp_file(tmp_filepath)) {
		strcpy(CurrentErrorMessage, "Failed to create tmp file");
		return FALSE;
	}
	printf("mktemp: %s\n", tmp_filepath);

	// TODO cmd_buffer should be bigger than the tmp_filepath
	// maybe I need to change something here
	char cmd_buffer[STRING_SIZE * 2] = { 0 };
	// TODO check output of sprintf
	sprintf(cmd_buffer, "gpg -d %s > %s", filepath, tmp_filepath);
	if (system(cmd_buffer) != 0) {
		strcpy(CurrentErrorMessage, "Failed to decrypt file");
		return FALSE;
	}

	char hash_original[STRING_SIZE] = { 0 };
	if (!md5_hash_filepath(hash_original, tmp_filepath)) {
		strcpy(CurrentErrorMessage, "Failed to get original file hash");
		// TODO either delete the file or print this error as warning
		return FALSE;
	}

	char command[OUTPUT_BUFFER_SIZE] = { 0 };
	if (!format_command_open_file(command, config, tmp_filepath)) {
		strcpy(CurrentErrorMessage,
		       "Failed to format command to open decrypted file");
		return FALSE;
	}

	if (run_command(command) != 0) {
		strcpy(CurrentErrorMessage, "Failed to open decrypted file");
		return FALSE;
	}

	char hash_updated[STRING_SIZE] = { 0 };
	if (!md5_hash_filepath(hash_updated, tmp_filepath)) {
		strcpy(CurrentErrorMessage, "Failed to get updated file hash");
		// TODO either delete the file or print this error as warning
		return FALSE;
	}

	if (strcmp(hash_original, hash_updated) != 0) {
		// TODO it asks everytime to replace original file
		sprintf(cmd_buffer, "gpg -e -r %s -o %s %s",
			DEFAULT_GPG_RECIPIENT, filepath, tmp_filepath);
		if (system(cmd_buffer) != 0) {
			strcpy(CurrentErrorMessage, "Failed to reencrypt file");
			return FALSE;
		}
	}

	sprintf(cmd_buffer, "shred -u %s", tmp_filepath);
	if (system(cmd_buffer) != 0) {
		strcpy(CurrentErrorMessage, "Failed to shred tmp file");
		return FALSE;
	}

	return TRUE;
}

int main(int argn, char **argv)
{
	char config_filepath[STRING_SIZE] = { 0 };
	if (!get_config_filepath(config_filepath)) {
		printf("config file not found\n");
		return 1;
	}

	printf("config filepath: %s\n", config_filepath);
	if (argn < 2) {
		printf("usage:\n\t%s <filepath>\n", argv[0]);
		return 1;
	}

	Config config;
	if (!parse_config_file(&config, config_filepath) != 0) {
		printf("ERROR: config file not found\n");
		return 1;
	}

	if (argn == 3 && strcmp("-d", argv[1]) == 0) {
		if (!decrypt_reencrypt_pgp(config, argv[2])) {
			printf("ERROR: %s\n", CurrentErrorMessage);
			return 1;
		}
		return 0;
	}

	for (size_t i = 0; i < config.size; i++) {
		printf("config %ld: %s = %s\n", i, config.key_values[i].key,
		       config.key_values[i].value);
	}

	char *filepath = argv[1];
	char final_command[OUTPUT_BUFFER_SIZE] = { 0 };

	if (!format_command_open_file(final_command, config, filepath)) {
		printf("ERROR: don't know how to open\n");
		return 1;
	}

	printf("Final command: %s\n", final_command);

	return run_command(final_command);
}

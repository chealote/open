#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
// #include<assert.h>
#include<unistd.h>

#define KEY_VALUES_SIZE 50
#define STRING_SIZE 100
#define OUTPUT_BUFFER_SIZE 1000

#define TRUE 1
#define FALSE 0

typedef struct {
	char key[KEY_VALUES_SIZE];
	char value[KEY_VALUES_SIZE];
} KeyValue;

typedef struct {
	KeyValue key_values[KEY_VALUES_SIZE];
	size_t size;
} Config;

char parse_line(KeyValue * kv, char *line)
{
	char *c = line;
	char reading_value = 0;
	char *pointer = kv->key;

	while (*c != '\0') {
		if (*c == '=') {
			// switch to value
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
	FILE *p = popen(command, "r");
	int status = pclose(p);
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
	return dot + 1;
}

char get_filetype(char *filetype, char *filename)
{
	char buffer[OUTPUT_BUFFER_SIZE] = { 0 };
	sprintf(buffer, "file -b \"%s\"", filename);

	if (run_save_stdout(buffer, filetype) != 0) {
		return FALSE;
	}

	return TRUE;
}

int match_filetype_with_config(Config config, char *filename)
{
	/* TODO maybe some files should be opened directly from the
	 * extension
	 *
	 *  const char *ext = get_filename_ext(filename);
	 *  printf("file ext: %s\n", ext);
	 *  if (strcmp(ext, "html") == 0) {
	 *  return F_HTML;
	 *  } 
	 */

	char filetype[OUTPUT_BUFFER_SIZE] = { 0 };
	if (!get_filetype(filetype, filename)) {
		printf("ERROR: failed to get filetype from %s\n", filename);
		return -1;
	}

	printf("filename=%s\nfile output=%s\n", filename, filetype);

	for (size_t i = 0; i < config.size; i++) {
		if (strstr(filetype, config.key_values[i].key)) {
			return i;
		}
	}

	return -1;
}

char get_config_filepath(char *output)
{
	char *path_prefix = getenv("HOME");

	char *path_suffixs[3] = {
		".config",
		"Programming/c/open",
	};

	for (int i = 0; i < 3; i++) {
		char *config_filename = "config.cfg";
		// TODO it returns something, check that
		sprintf(output, "%s/%s/%s", path_prefix, path_suffixs[i],
			config_filename);
		if (access(output, F_OK) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

int main(int argn, char **argv)
{
	char config_filepath[STRING_SIZE] = { 0 };
	if (!get_config_filepath(config_filepath)) {
		printf("config file not found\n");
		return 1;
	}

	printf("config filepath: %s\n", config_filepath);
	if (argn != 2) {
		printf("usage:\n\t%s <filepath>\n", argv[0]);
		return 1;
	}

	Config config;
	if (!parse_config_file(&config, config_filepath) != 0) {
		printf("ERROR: config file not found\n");
		return 1;
	}

	for (size_t i = 0; i < config.size; i++) {
		printf("config %ld: %s = %s\n", i, config.key_values[i].key,
		       config.key_values[i].value);
	}

	char *filepath = argv[1];
	char final_command[OUTPUT_BUFFER_SIZE] = { 0 };

	int config_index = match_filetype_with_config(config, filepath);
	if (config_index == -1) {
		printf("ERROR: don't know how to open\n");
		return 1;
	}

	printf("Matched config: key=%s value=%s\n",
	       config.key_values[config_index].key,
	       config.key_values[config_index].value);
	sprintf(final_command, "%s %s", config.key_values[config_index].value,
		filepath);
	printf("Final command: %s\n", final_command);

	return run_command(final_command);
}

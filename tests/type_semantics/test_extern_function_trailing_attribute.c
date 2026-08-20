typedef int sdk_bool;

extern sdk_bool sdk_function(const char *path)
	__attribute__((availability(macos, introduced=10.5)));

int main(void)
{
	return 0;
}

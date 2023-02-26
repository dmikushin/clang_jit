#include <string>
#include <vector>

#include <llvm/ADT/StringRef.h>

std::vector<std::string> getClangCC1Args(llvm::StringRef cpp, llvm::StringRef bc)
{
	std::vector<std::string> args;

	args.push_back("-emit-llvm");
	args.push_back("-emit-llvm-bc");
	args.push_back("-emit-llvm-uselists");

	args.push_back("-main-file-name");
	args.push_back(cpp.data());

	args.push_back("-std=c++14");
	args.push_back("-disable-free");
	args.push_back("-fdeprecated-macro");

	args.push_back("-mrelocation-model");
	args.push_back("static");
	args.push_back("-mthread-model");
	args.push_back("posix");
	args.push_back("-mconstructor-aliases");
	args.push_back("-munwind-tables");

	args.push_back("-debugger-tuning=gdb");

#if NDEBUG
	args.push_back("-O3");
	args.push_back("-ffast-math");
	args.push_back("-vectorize-loops");
	args.push_back("-vectorize-slp");
#else
	args.push_back("-O0");
	args.push_back("-fmath-errno");
	args.push_back("-debug-info-kind=limited");
	args.push_back("-dwarf-version=4");
#endif

	args.push_back("-o");
	args.push_back(bc.data());
	args.push_back("-x");
	args.push_back("c++");
	args.push_back(cpp.data());

	return args;
}


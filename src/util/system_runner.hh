#ifndef SYSTEM_RUNNER_HH
#define SYSTEM_RUNNER_HH

#include <vector>
#include <string>
#include <functional>

int ezexec( const std::vector< std::string > & command, const bool path_search = false );
void run( const std::vector< std::string > & command );

#endif /* SYSTEM_RUNNER_HH */

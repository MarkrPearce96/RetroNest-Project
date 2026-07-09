#include "github_credentials.h"

namespace GitHubCredentials {
QString token() {
#ifdef RETRONEST_GITHUB_TOKEN
    return QStringLiteral(RETRONEST_GITHUB_TOKEN);
#else
    return {};
#endif
}
}

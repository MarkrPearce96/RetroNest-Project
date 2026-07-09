#pragma once
#include <QString>

// The GitHub Personal Access Token compiled into private (dev) builds via
// dev_credentials.cmake → RETRONEST_GITHUB_TOKEN. Empty in any build without
// that gitignored config (e.g. a public build), which disables private-repo
// downloads cleanly. Mirrors ScraperCredentials' build-time secret pattern.
namespace GitHubCredentials {
QString token();
}

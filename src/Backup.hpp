#pragma once

namespace th06
{
void CreateGameBackup(bool force = false);
void PruneOldBackups(const char *backupDir, int keepCount);
} // namespace th06

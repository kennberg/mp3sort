//
// mp3sort -- created by Alex Kennberg, Jan 2003. akennberg@rogers.com
// Visible credit must be given to the author wherever the source is used.
// Permission for commercial use must be granted by the author.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include <string>
#include <vector>
using namespace std;

// Settings
#define EXTENSION_SIZE      4
#define LOG_FILE            "__mp3sort.log"

typedef struct playEntry_s
{
    bool    bFound;
    char    pcOriginal[MAX_PATH];
    int     nOrigLen;
    char    pcModified[MAX_PATH];
} playEntry_t;

typedef vector< playEntry_t >       playEntryList;

const char *    g_ppkcPlayListExt[] = { "m3u" };
const char *    g_ppkcMyExt[] = { "mp3" };
const int       g_knPlayListExtSize = sizeof(g_ppkcPlayListExt) / sizeof(const char *);
const int       g_knMyExtSize = sizeof(g_ppkcMyExt) / sizeof(const char *);

// Counters
int             g_nTotalFiles;
int             g_nTotalMatches;
int             g_nTotalChanged;
int             g_nTotalErrors;
int             g_nTotalPlayLists;
int             g_nTotalPlayListErrors;
int             g_nTotalPlayListsChanged;

void ChangeFile( const char *pkcTargetPath, const char *pkcFileName, playEntryList& playList)
{
    FILE*       pFile;
    char        pcOldPath[MAX_PATH];
    char        pcNewPath[MAX_PATH];
    bool        bChanged = false;
    char        x, y;
    int         i, j, k;
    char*       pcNewFileName;
    playEntry_t play;

    strcpy( play.pcOriginal, pkcFileName );
    play.nOrigLen = strlen( pkcFileName );

    // Copy over the complete path
    for (i = 0; pkcTargetPath[i]; i++)
    {
        pcOldPath[i] = pcNewPath[i] = pkcTargetPath[i];
    }

    // Append \ and move to next character
    pcOldPath[i] = pcNewPath[i] = '\\';
    k = ++i;
    pcNewFileName = pcNewPath + i;

    // Parse the file name
    for (j = 0; pkcFileName[j]; j++)
    {
        x = pcOldPath[k + j] = pkcFileName[j];
        y = pkcFileName[j + 1];

        // We must not allow double spaces
        if (x == ' ' && y == ' ')
        {
            if (!bChanged)
            {
                bChanged = true;
            }
            continue;
        }

        // We will change _ to space
        if (y == '_' && x != ' ')
        {
            y = ' ';
        }

        // Change _ to space
        if (x == '_' && y != ' ')
        {
            x = ' ';
        }

        // Dash must be surrounded by spaces
        else if (x == '-' && y != ' ')
        {
            pcNewPath[i] = '-';
            i++;
            x = ' ';
        }
        else if (y == '-' && x != ' ')
        {
            pcNewPath[i] = x;
            i++;
            x = ' ';
        }

        // Add new character to new name
        pcNewPath[i] = x;
        if (x != pcOldPath[k + j] && !bChanged)
        {
            bChanged = true;
        }

        i++;
    }

    pcOldPath[k + j] = 0;
    pcNewPath[i] = 0;

    strcpy( play.pcModified, pcNewFileName );

    if (bChanged)
    {
        // Attempt to rename
        if (MoveFile(pcOldPath, pcNewPath))
        {
            g_nTotalChanged++;
            playList.push_back(play);
        }
        else
        {
            g_nTotalErrors++;
            i = GetLastError();

            // Append errors to our log file
            if (pFile = fopen(LOG_FILE, "ab"))
            {
                if (i == ERROR_ALREADY_EXISTS)
                {
                    fprintf(    pFile,
                                "Exists: \r\n\t%s\r\n\t%s\r\n",
                                pcOldPath,
                                pcNewPath);
                }
                else
                {
                    fprintf(    pFile,
                                "Conflict (%d):\r\n\t%s\r\n\t%s\r\n",
                                i,
                                pcOldPath,
                                pcNewPath);
                }

                fclose(pFile);
            }
        }
    }
}

bool HandleFile( const char *pkcTargetPath, const char *pkcFileName, playEntryList& playList)
{
    char    pcExt[EXTENSION_SIZE + 1];
    bool    bFoundDot = false;
    bool    bFoundExt = false;
    int     i, j;
    bool    rtnVal = false;

    // Find extension
    for(i = 0; pkcFileName[i]; i++)
    {
        if (pkcFileName[i] == '.')
        {
            bFoundDot = true;
            j = 0;
        }
        else if(bFoundDot)
        {
            // If extension too long then we ignore it
            if (j >= EXTENSION_SIZE)
            {
                bFoundDot = false;
            }
            else
            {
                pcExt[j] = pkcFileName[i];
                j++;
            }
        }
    }

    if (bFoundDot)
    {
        pcExt[j] = 0;

        // Ignore unwanted extensions
        for (i = 0; i < g_knMyExtSize; i++)
        {
            if (!strcmp(pcExt, g_ppkcMyExt[i]))
            {
                bFoundExt = true;
                break;
            }
        }

        if (bFoundExt)
        {
            g_nTotalMatches++;
            ChangeFile(pkcTargetPath, pkcFileName, playList);
        }
        else
        {
            // search for a play list extension
            for (i = 0; i < g_knPlayListExtSize; i++)
            {
                if (!strcmp(pcExt, g_ppkcPlayListExt[i]))
                {
                    rtnVal = true;
                    g_nTotalPlayLists++;
                    break;
                }
            }
        }
    }

    return rtnVal;
}

void ParsePlayList( const char* pkcTargetPath, playEntryList& playList )
{
    FILE*   pFile;
    char*   pcBuffer;
    int     nSize;
    int     i, p;
    bool    bChanged = false;

    pFile = fopen( pkcTargetPath, "rb" );
    if ( !pFile )
    {
        g_nTotalPlayListErrors++;
        return;
    }

    // get file length
    fseek( pFile, 0, SEEK_END );
    nSize = (int) ftell( pFile );
    fseek( pFile, 0, SEEK_SET);

    // read it to buffer
    pcBuffer = new char[ nSize + 1 ];
    nSize = fread( pcBuffer, 1, nSize, pFile );
    if ( nSize <= 0 )
    {
        g_nTotalPlayListErrors++;
        fclose( pFile );
        return;
    }
    fclose( pFile );

    // open play list for write
    pFile = fopen( pkcTargetPath, "wb" );
    if ( !pFile )
    {
        g_nTotalPlayListErrors++;
        return;
    }

    for( i = 0; i < playList.size(); ++i )
        playList[i].bFound = false;

    // parse the play list
    for( p = 0; p < nSize; )
    {
        for( i = 0; i < playList.size(); ++i )
        {
            if( playList[i].bFound )
                continue;
            if( !strncmp( pcBuffer + p, playList[i].pcOriginal, playList[i].nOrigLen ) )
            {
                bChanged = true;
                playList[i].bFound = true;

                fprintf( pFile, "%s", playList[i].pcModified );
                p += strlen( playList[i].pcOriginal );
                break;
            }
        }
        if( i == playList.size() )
        {
            fprintf( pFile, "%c", pcBuffer[p] );
            p++;
        }
    }

    fclose(pFile);
    delete []pcBuffer;

    if( bChanged )
        g_nTotalPlayListsChanged++;
}

void FindFiles(char *pcTargetPath)
{
    WIN32_FIND_DATA     FindData;
    HANDLE              hFile;
    char                pcPath[MAX_PATH];
    playEntryList       playList;
    vector< string >    vecPlayListPaths;
    vector< string >    vecFileNames;
    int                 i;

    printf("Target: %s\n", pcTargetPath);

    strcpy(pcPath, pcTargetPath);
    strcat(pcPath, "\\*");

    // Find all files in this path
    hFile = FindFirstFile(pcPath, &FindData);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (FindData.cFileName[0] != '.')
            {
                // Found directory
                if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    // We can re-use the original variable
                    sprintf(pcPath, "%s\\%s", pcTargetPath, FindData.cFileName);
                    FindFiles(pcPath);
                }
                // Found a file
                else
                {
                    string str( FindData.cFileName );
                    vecFileNames.push_back( str );
                    g_nTotalFiles++;
                }
            }
        } while (FindNextFile(hFile, &FindData));

        FindClose(hFile);
    }

    for( i = 0; i < vecFileNames.size(); ++i )
    {
        if( HandleFile( pcTargetPath, vecFileNames[i].c_str(), playList ) )
        {
            sprintf( pcPath, "%s\\%s", pcTargetPath, vecFileNames[i].c_str() );
            {
                string str( pcPath );
                vecPlayListPaths.push_back( str );
            }
        }
    }


    if( vecPlayListPaths.size() > 0 && playList.size() > 0 )
    {
        for( i = 0; i < vecPlayListPaths.size(); ++i )
            ParsePlayList( vecPlayListPaths[i].c_str(), playList );
    }
}

int main()
{
    char    pcPath[MAX_PATH];

    printf( "mp3sort v1.1\n"
            "\t-- created by Alex Kennberg, Jan 2003.\n"
            "\t-- updated Sep 2005.\n"
            "\t-- akennberg@rogers.com\n\n"
            "Changes the names of files from \"Band_Name-Song   Title.mp3\" to\n"
            "\"Band Name - Song Title.mp3\" in the current directory and all\n"
            "subdirectories of where the program is ran. Modifies play lists\n"
            "in the subdirectory with the new file names.\n\n");

    // Init counters
    g_nTotalFiles = 0;
    g_nTotalMatches = 0;
    g_nTotalChanged = 0;
    g_nTotalErrors = 0;

    // Make the file name
    GetCurrentDirectory(sizeof(pcPath), pcPath);

    // Search main dir
    FindFiles(pcPath);

    printf( "Total: % 6d files\n"
            "       % 6d music files\n"
            "       % 6d music file errors\n"
            "       % 6d music files changed\n"
            "       % 6d play lists\n"
            "       % 6d play list errors\n"
            "       % 6d play lists changed\n",
            g_nTotalFiles,
            g_nTotalMatches,
            g_nTotalErrors,
            g_nTotalChanged,
            g_nTotalPlayLists,
            g_nTotalPlayListErrors,
            g_nTotalPlayListsChanged);

    // Pause for the user
    system("pause");
}

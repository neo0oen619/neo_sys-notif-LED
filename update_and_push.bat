@echo off
setlocal

REM Change to the folder where this script is located
cd /d "%~dp0"

REM Ensure this is a git repo (init if needed)
if not exist ".git" (
    echo [.INFO] .git folder not found. Initializing a new git repository here...
    git init
)

REM Get commit message from arguments or ask the user
set COMMIT_MSG=%*
if "%COMMIT_MSG%"=="" (
    set /p COMMIT_MSG=Enter commit message: 
)

if "%COMMIT_MSG%"=="" (
    echo [ERROR] No commit message provided.
    pause
    exit /b 1
)

echo.
echo This script will take the files in this folder and push them to GitHub.
echo It does NOT pull from GitHub first.

REM Ask for GitHub username and token
echo.
set /p GH_USER=GitHub username (without @github.com): 
set /p GH_TOKEN=GitHub personal access token (visible as you type): 

if "%GH_USER%"=="" (
    echo [ERROR] No username provided.
    pause
    exit /b 1
)

if "%GH_TOKEN%"=="" (
    echo [ERROR] No token provided.
    pause
    exit /b 1
)

REM Get current branch name (fall back to main if empty)
set BRANCH=
git rev-parse --abbrev-ref HEAD > "%TEMP%\sys-notif-LED-branch.txt" 2>nul
if exist "%TEMP%\sys-notif-LED-branch.txt" (
    set /p BRANCH=<"%TEMP%\sys-notif-LED-branch.txt"
    del "%TEMP%\sys-notif-LED-branch.txt" >nul 2>&1
)
if "%BRANCH%"=="" (
    set BRANCH=main
)

echo.
echo Current branch: %BRANCH%

echo.
echo Staging all changes in this folder...
git add -A

REM Check if anything is staged
git diff --cached --quiet
if %errorlevel%==0 (
    echo.
    echo No changes to commit.
    pause
    exit /b 0
)

echo.
echo Committing with message: "%COMMIT_MSG%"
git commit -m "%COMMIT_MSG%"
if errorlevel 1 (
    echo.
    echo [ERROR] git commit failed.
    pause
    exit /b 1
)

echo.
echo Pushing HEAD to the main branch on GitHub using the username and token you entered...
echo Repo: https://github.com/neo0oen619/sys-notif-LED.git (HEAD -> main, force)
git push "https://%GH_USER%:%GH_TOKEN%@github.com/neo0oen619/sys-notif-LED.git" HEAD:main --force
REM If you want to avoid overwriting remote history, remove the --force flag above.

if errorlevel 1 (
    echo.
    echo [ERROR] git push failed.
    pause
    exit /b 1
)

echo.
echo ==========================================
echo ✅ Done! GitHub now has a commit with the files from this folder.
echo ==========================================
pause
endlocal
exit /b 0

#include <curses.h>
#include <stdlib.h>

#include <vector>
#include <regex>
#include <string>
#include <algorithm>

using namespace std;

#define MAX_BRANCH_SHOWN 20
#define MAX_BRANCH_NAME_LENGTH 200

#define NEW_LINE 10


int kbhit(void){
    int ch = getch();
    if (ch != ERR) {
        ungetch(ch);
        return 1;
    } else {
        return 0;
    }
}

vector<string> get_branches(){
    // naughty fix to get raw names, no origin/*
    // depends on remote name, works for me
    string command_all_branches = "git for-each-ref --format='%(refname:short)' | sed 's/origin\\///g'";
    FILE *pp;
    vector<string> branches;
    char buffer[MAX_BRANCH_NAME_LENGTH];
    if ((pp = popen(command_all_branches.c_str(), "r")) != 0) {
        while (fgets(buffer, 10000, pp) != 0) {
            string branch_name = string(buffer);
            if(! count(branches.begin(), branches.end(), branch_name)) branches.push_back(branch_name);
        }
        pclose(pp);
    }
    return branches;
}

void print_window(char* regex_value, vector<string*> filtered_branches, int selected_branch){
    erase();

    // selection may be out of range because of new regex
    int lines = filtered_branches.size();
    if(lines && selected_branch >= lines) selected_branch = lines - 1;

    init_pair(1, -1, -1);
    init_pair(2, COLOR_RED, -1);

    // print regex
    printw("regex: %s | %d\n\n", regex_value, lines);

    // print branches
    int i = 0;
    for (; i < filtered_branches.size() && i < MAX_BRANCH_SHOWN; ++i){
        addstr("  ");
        if(i == selected_branch) attrset(COLOR_PAIR(2));
        addstr(filtered_branches[i]->c_str());
        if(i == selected_branch) attrset(COLOR_PAIR(1));
    }
    if(i < filtered_branches.size()) addstr("...");

    // marker on selected branch
    if(lines > 0) mvaddstr(selected_branch + 2, 0, ">");
    refresh();
}

void switch_to_branch(string branch_name, bool pull_after_checkout){
    endwin();
    string command = "git checkout " + branch_name;
    system(command.c_str());
    if (pull_after_checkout) {
        system("git pull");
    }
    exit(0);
}

void filter_branches(vector<string> &all_branches, char* regex_value, vector<string*> &filtered_branches){
    filtered_branches.clear();
    try{
        regex regex_(regex_value, std::regex_constants::icase);
        for (int i = 0; i < all_branches.size(); ++i){
            if (regex_search(all_branches[i], regex_)){
                filtered_branches.push_back(&all_branches[i]);
            }
        }
    } catch(exception e){
        return;
    }
}

bool update_branches(int argc, char** argv){
    if (argc <= 1) return false;

    bool pull_after_checkout = false;
    for (int i = 1; i < argc; i++) {  // ignore program name
        string arg = argv[i];
        if (arg == "-u") system("git fetch");
        else if (arg == "-p") pull_after_checkout = true;
    }
    return pull_after_checkout;
}

int main(int argc, char** argv)
{
    bool pull_after_checkout = update_branches(argc, argv);

    // setup curses
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    curs_set(0);
    scrollok(stdscr, TRUE);
    keypad(stdscr, TRUE);
    use_default_colors();
    start_color();

    // variables
    char regex_value[MAX_BRANCH_NAME_LENGTH] = {0};
    int index = 0;
    int selected_branch = 0;
    vector<string> all_branches = get_branches();
    vector<string*> filtered_branches;
    for (int i = 0; i < all_branches.size(); ++i){
        filtered_branches.push_back(&all_branches[i]);
    }

    print_window(regex_value, filtered_branches, selected_branch);

    // handle keyboard input and loop until a branch is selected
    while (1) {
        if (kbhit()) {
            int c = getch();
            if(c == KEY_BACKSPACE){
                if(index > 0){
                    index--;
                    regex_value[index] = 0;
                }
            }else if (c == NEW_LINE){
                if(filtered_branches.size()) switch_to_branch(*filtered_branches[selected_branch], pull_after_checkout);
            }else if (c == KEY_UP){
                if(selected_branch > 0) selected_branch--;
            }else if (c == KEY_DOWN){
                if(selected_branch < filtered_branches.size() && selected_branch < MAX_BRANCH_SHOWN - 1) selected_branch++;
            }else{
                // TODO: handle other keys
                regex_value[index] = c;
                index++;
            }
            filter_branches(all_branches, regex_value, filtered_branches);
            print_window(regex_value, filtered_branches, selected_branch);

        }
    }
}

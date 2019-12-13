#include <curses.h>
#include <stdlib.h>

#include <vector>
#include <unordered_map>
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

struct branch_t{
    string name;
    bool local, remote;
};

vector<branch_t> get_branches(){
    char buffer[MAX_BRANCH_NAME_LENGTH];

    unordered_map<string, branch_t> branches;
    branch_t branch;
    FILE *pp = popen("git branch -l", "r");
    while (fgets(buffer, MAX_BRANCH_NAME_LENGTH, pp) != 0) {
        string branch_name = string(buffer);
        branch_name = regex_replace(branch_name, std::regex("(^ +)|(\\* )|\n"), "");
        branch.name = branch_name;
        branch.local = true;
        branch.remote = false;
        branches[branch.name] = branch;
    }
    pclose(pp);

    pp = popen("git branch -r", "r");
    while (fgets(buffer, MAX_BRANCH_NAME_LENGTH, pp) != 0) {
        string branch_name = string(buffer);
        branch_name = regex_replace(branch_name, std::regex("(^ +)|(\\* )|\n"), "");
        // remove remote's name
        branch_name = branch_name.substr(branch_name.find('/')+1, branch_name.size());
        if(!branches.count(branch_name)){
            branch.name = branch_name;
            branch.local = false;
           branches[branch_name] = branch;
        }
        branches[branch_name].remote = true;
    }
    pclose(pp);

    vector<branch_t> res;
    for(auto &key_val : branches) res.push_back(key_val.second);
    return res;
}

void print_window(char* regex_value, vector<branch_t*> filtered_branches, int selected_branch){
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
        addstr(filtered_branches[i]->name.c_str());
        if(filtered_branches[i]->remote && not filtered_branches[i]->local)
            addstr(" (R)");
        addstr("\n");
        if(i == selected_branch) attrset(COLOR_PAIR(1));
    }
    if(i < filtered_branches.size()) addstr("...");

    // marker on selected branch
    if(lines > 0) mvaddstr(selected_branch + 2, 0, ">");
    refresh();
}

void switch_to_branch(branch_t branch, bool pull_after_checkout){
    endwin();
    string command = "git checkout " + branch.name;
    system(command.c_str());
    if (pull_after_checkout) {
        system("git pull");
    }
    exit(0);
}

void filter_branches(vector<branch_t> &all_branches, char* regex_value, vector<branch_t*> &filtered_branches){
    filtered_branches.clear();
    try{
        regex regex_(regex_value, regex_constants::icase);
        for (int i = 0; i < all_branches.size(); ++i){
            if (regex_search(all_branches[i].name, regex_)){
                filtered_branches.push_back(&all_branches[i]);
            }
        }
    } catch(exception e){
        return;
    }
}

bool best_match(vector<branch_t> &all_branches, char* pattern, branch_t& res){
    // pattern to lowercase
    transform(pattern, pattern+strlen(pattern), pattern, ::tolower);

    auto better_match = [pattern]( const string& a, const string& b) {
        // the one starting with the pattern is prefered
        if(a.find(pattern) == 0 and b.find(pattern) != 0) return true;
        if(b.find(pattern) == 0 and a.find(pattern) != 0) return false;

        // the one closer in length to the pattern is prefered
        if(a.size()-strlen(pattern) < b.size()-strlen(pattern)) return true;
        if(b.size()-strlen(pattern) < a.size()-strlen(pattern)) return false;

        // tiebreaker
        return a < b;
    };

    int index = -1;
    for (int i = 0; i< all_branches.size(); i++){
        string b = all_branches[i].name;
        // branch to lowercase
        transform(b.begin(), b.end(), b.begin(), ::tolower);
        if (b.find(pattern) != string::npos) {
            if(index == -1 or better_match(b, res.name)){
                index = i;
                res = all_branches[i];
            }
        }
    }

    if(index != -1) res = all_branches[index];
    return index != -1;
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

bool fast_switch(int argc, char** argv, char* regex_value){
    for (int i = 1; i < argc; i++) {  // ignore program name
        if (argv[i][0] != '-'){
            strcpy(regex_value, argv[i]);
            return true;
        }
    }
    return false;
}

int main(int argc, char** argv)
{
    bool pull_after_checkout = update_branches(argc, argv);

    // variables
    char regex_value[MAX_BRANCH_NAME_LENGTH] = {0};
    char pattern[MAX_BRANCH_NAME_LENGTH] = {0};
    int index = 0;
    int selected_branch = 0;
    vector<branch_t> all_branches = get_branches();
    vector<branch_t*> filtered_branches;
    for (int i = 0; i < all_branches.size(); ++i){
        filtered_branches.push_back(&all_branches[i]);
    }

    if (fast_switch(argc, argv, pattern)){
        branch_t target_branch;
        if (best_match(all_branches, pattern, target_branch)){
            switch_to_branch(target_branch, pull_after_checkout);
        } else {
            fprintf(stderr, "No branches matching '%s'\n", pattern);
            exit(1);
        }
    }

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

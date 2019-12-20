#include <curses.h>
#include <stdlib.h>

#include <vector>
#include <unordered_map>
#include <regex>
#include <string>
#include <algorithm>

using namespace std;

#define MAX_BRANCH_NAME_LENGTH 500
#define MAX_LINE_LEN MAX_BRANCH_NAME_LENGTH
#define NEW_LINE 10

int max_branch_shown = -1;

int kbhit(void){
    int ch = getch();
    if (ch != ERR) {
        ungetch(ch);
        return 1;
    } else {
        return 0;
    }
}

int run_command(const string& command, vector<string>* out_lines=nullptr, bool show_failure=true){
    vector<string> _out_lines;
    char buff[MAX_LINE_LEN];
    FILE *f = popen(command.c_str(), "r");
    while (!feof(f)) if (fgets(buff, sizeof(buff), f) != NULL)
        _out_lines.push_back(buff);
    int status = WEXITSTATUS(pclose(f));
    if(status and show_failure){
        erase();
        for(auto &l : _out_lines) addstr(l.c_str());
        addstr("\n\nPress any key to continue.");
        while(!kbhit());
        getch();
    }
    if(out_lines) *out_lines = _out_lines;
    return status;
}

struct branch_t{
    string name;
    bool local, remote;
};

vector<branch_t> get_branches(bool locals_only){
    unordered_map<string, branch_t> branches;
    branch_t branch;
    vector<string> out_lines;

    run_command("git branch -l", &out_lines);
    for(string& branch_name : out_lines){
        branch.name = regex_replace(branch_name, std::regex("(^ +)|(\\* )|\n"), "");
        branch.local = true;
        branch.remote = false;
        branches[branch.name] = branch;
    }

    if(not locals_only){
        run_command("git branch -r", &out_lines);
        for(string& branch_name : out_lines){
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
    }

    vector<branch_t> res;
    for(auto &key_val : branches) if(key_val.second.local) res.push_back(key_val.second);
    for(auto &key_val : branches) if(!key_val.second.local) res.push_back(key_val.second);
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
    for (; i < filtered_branches.size() && i < max_branch_shown; ++i){
        addstr("  ");
        if(i == selected_branch) attrset(COLOR_PAIR(2));
        addstr(filtered_branches[i]->name.c_str());
        if(filtered_branches[i]->remote && not filtered_branches[i]->local)
            addstr(" [R]");
        addstr("\n");
        if(i == selected_branch) attrset(COLOR_PAIR(1));
    }
    if(i < filtered_branches.size()) addstr("...");

    // marker on selected branch
    if(lines > 0) mvaddstr(selected_branch + 2, 0, ">");
    refresh();
}

void delete_branch(vector<branch_t*> &branches, int target_index){
    erase();
    string branch_name = branches[target_index]->name;
    printw("Delete '%s'? [Y/N]\n", branch_name.c_str());
    while (1) {
        if (kbhit()) {
            int c = getch();
            if(c == 'Y' or c == 'y') run_command("git branch -d " + branch_name + " 2>&1");
            break;
        }
    }
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

struct args_t{
    char* fast_switch = NULL;
    bool pull_after=false, fetch_before=false, locals_only=false;
    args_t(int argc, char** argv) {
        for (int i = 1; i < argc; i++) {
            string arg = argv[i];
            if (arg[0] != '-') fast_switch = argv[i];
            if (arg == "-u") fetch_before = true;
            if (arg == "-p") pull_after = true;
            if (arg == "-l") locals_only = true;
        }
    }
};

int main(int argc, char** argv){
    args_t args(argc, argv);

    if (args.fetch_before) system("git fetch");

    // variables
    char regex_value[MAX_BRANCH_NAME_LENGTH] = {0};
    int index = 0;
    int selected_branch = 0;
    vector<branch_t> all_branches = get_branches(args.locals_only);
    vector<branch_t*> filtered_branches;
    for (int i = 0; i < all_branches.size(); ++i){
        filtered_branches.push_back(&all_branches[i]);
    }

    if (args.fast_switch != NULL){
        branch_t target_branch;
        if (best_match(all_branches, args.fast_switch, target_branch)){
            switch_to_branch(target_branch, args.pull_after);
        } else {
            endwin();
            fprintf(stderr, "No branches matching '%s'\n", args.fast_switch);
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

    int max_height, max_width;
    getmaxyx(stdscr, max_height, max_width);
    max_branch_shown = max_height - 5;

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
                if(filtered_branches.size()) switch_to_branch(*filtered_branches[selected_branch], args.pull_after);
            }else if (c == KEY_UP){
                if(selected_branch > 0) selected_branch--;
            }else if (c == KEY_DOWN){
                if(selected_branch < filtered_branches.size()-1 && selected_branch < max_branch_shown - 1) selected_branch++;
            }else if (c == KEY_DC){
                if(filtered_branches.size() and filtered_branches[selected_branch]->local){
                    delete_branch(filtered_branches, selected_branch);
                    all_branches = get_branches(args.locals_only);
                }
            }else{
                regex_value[index] = c;
                index++;
            }
            filter_branches(all_branches, regex_value, filtered_branches);
            print_window(regex_value, filtered_branches, selected_branch);
        }
    }
}

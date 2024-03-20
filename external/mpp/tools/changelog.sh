#!/bin/bash
## Author LinkinStar / HermanChen

# solve the space by IFS
OLDIFS=$IFS
IFS=`echo -en "\n\b"`
echo -en $IFS

declare -g changelog_file="CHANGELOG.md"
declare -g git_repo_rootdir
declare -g current_tag
declare -g previous_tag
declare -g prev_changelog
declare -g prev_changelog_version
declare -g prev_tag
declare -g curr_tag

# changelog version define
declare -g version_major
declare -g version_minor
declare -g version_patch

function log_filter_and_print() {
    local title=$1
    local prefix=$2
    local -a msgs=()
    local msg_cnt=0
    local prefix_len=${#prefix}

    # filter log and find the matched prefix number
    for msg in ${orig_logs[@]}
    do
        if [[ $msg == "$prefix"* ]]; then
            msgs[msg_cnt]=$msg
            let msg_cnt++

            echo "$prefix commit $msg_cnt - ${msg}"
        fi
    done

    if [ $msg_cnt -gt 0 ]; then
        echo -e "### $title" >> ${changelog_file}

        for msg in ${msgs[@]}
        do
            # check log mode
            local pos=$prefix_len
            local log

            # message mode: prefix: xxxx
            log=$(echo $msg | grep -i "^$prefix:")
            #echo "pos $pos log $log"
            if [ "$log" != "" ]; then
                let pos++
            fi

            # remove extra space
            log=$(echo ${msg:$pos} | sed -e 's/^[ ]*//g' | sed -e 's/[ ]*$//g')
            echo -e "- $log" >> ${changelog_file}
        done

        echo -e "" >> ${changelog_file}
    fi
}

usage="changelog helper script
  use --check to check the latest commit message using angularcommits rule
  the basic commit message should be structured as follows:
  <type>[optional scope]: <description>
  [optional body]
  [optional footer(s)]

  Type
  Must be one of the following:
  feat: A new feature
  fix: A bug fix
  docs: Documentation only changes
  style: Changes that do not affect the meaning of the code (white-space, formatting, missing semi-colons, etc)
  refactor: A code change that neither fixes a bug nor adds a feature
  perf: A code change that improves performance
  test: Adding missing or correcting existing tests
  chore: Changes to the build process or auxiliary tools and libraries such as documentation generation

  https://www.conventionalcommits.org/zh-hans/v1.0.0/

  use --new version number to update CHANGELOG.md by insert new version on top
  the version infomation should contain the following infomation
"
#  .option('-p, --patch', 'create a patch changelog')
#  .option('-m, --minor', 'create a minor changelog')
#  .option('-M, --major', 'create a major changelog')
#  .option('-t, --tag <range>', 'generate from specific tag or range (e.g. v1.2.3 or v1.2.3..v1.2.4)')
#  .option('-x, --exclude <types>', 'exclude selected commit types (comma separated)', list)
#  .option('-f, --file [file]', 'file to write to, defaults to ./CHANGELOG.md, use - for stdout', './CHANGELOG.md')
#  .option('-u, --repo-url [url]', 'specify the repo URL for commit links')

while [ $# -gt 0 ]; do
    case $1 in
        --help | -h)
            echo "$usage"
            exit 1
            ;;
        --check)
            # check the latest commit message is following the rule or not
            shift
            ;;
        --major | -m)
            version_major=$2
            shift
            ;;
        --minor | -n)
            version_minor=$2
            shift
            ;;
        --patch | -p)
            version_patch=$2
            shift
            ;;
        --file | -f)
            changelog_file=$2
            echo "save changelog to file ${changelog_file}"
            shift
            ;;
        --repo-url | -u)
            git_repo_rootdir=$2
            echo "set repo rootdir to ${git_repo_rootdir}"
            shift
            ;;
        *)
            # skip invalid option
            shift
            ;;
    esac
    shift
done

# check git command
if ! command -v git >/dev/null; then
    >&2 echo "ERROR: git command is not available"
    exit 1
fi

# if repo root is not set set sefault repo root
if [ -z $git_repo_rootdir ]; then
    git_repo_rootdir=$(git rev-parse --show-toplevel 2>/dev/null)
fi

# check repo root valid or not
if [ -z ${git_repo_rootdir} ] || [ ! -d ${git_repo_rootdir} ]; then
    echo "ERROR: can not found repo root"
    exit 1
fi

# cd to rootdir
cd $git_repo_rootdir

# if changelog file is not exist then create it
if [[ ! -f ${changelog_file} ]]; then
    touch ${changelog_file}
else
    # get old changelog
    prev_changelog=$(cat $changelog_file)
    prev_changelog_version=$(echo $prev_changelog | grep -E "## *.*.* *" | head -1 | awk '{ print $2 }')

    rm -rf ${changelog_file}
    touch ${changelog_file}
fi

# get current version info
curr_ver=$(git describe --tags --long)
curr_tag=$(git describe --tags --abbrev=0)
prev_tag=$(git describe --tags --abbrev=0 "$curr_tag"^)
date="$(date '+%Y-%m-%d')"

echo "curr ver: ${curr_ver}"
echo "curr tag: ${curr_tag}"
echo "prev tag: ${prev_tag}"

# dump log between two tags
orig_logs=$(git log --pretty="%s" --no-merges $curr_tag...$prev_tag)
echo "commits: from ${prev_tag} to ${curr_tag}"
echo "${orig_logs}"

# check unreleased version number
unreleased_count=$(echo ${curr_ver} | grep -E "## *.*.* *" | head -1 | awk -F "-" '{ printf $2 }')
echo "unreleased version count: ${unreleased_count}"

# write unreleased information
if [ "$unreleased_count" ] && [ $unreleased_count -gt 0 ]; then
    echo -e "## Unreleased (${date})\n" >> ${changelog_file}

    unreleased_logs=$(git log --pretty="%s" --no-merges HEAD...$curr_tag)
    for msg in ${unreleased_logs[@]}
    do
        echo -e "- ${msg}" >> ${changelog_file}
    done

    echo -e "" >> ${changelog_file}
else
    echo -e "## $curr_tag ($date)" >> ${changelog_file}
fi

# write taged information
log_filter_and_print "Feature"  "feat"
log_filter_and_print "Fix"      "fix"
log_filter_and_print "Docs"     "docs"
log_filter_and_print "Style"    "Style"
log_filter_and_print "Refactor" "refactor"
log_filter_and_print "Perf"     "perf"
log_filter_and_print "Test"     "test"
log_filter_and_print "Chore"    "chore"

echo -e "${prev_changelog}" >> ${changelog_file}

IFS=$OLDIFS

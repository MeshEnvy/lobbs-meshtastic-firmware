import configparser
import subprocess
import os
run_number = os.getenv('GITHUB_RUN_NUMBER', '0')
build_location = os.getenv('BUILD_LOCATION', 'local')

def readProps(prefsLoc):
    """Read the version of our project as a string"""

    config = configparser.RawConfigParser()
    config.read(prefsLoc)
    version = dict(config.items("VERSION"))
    verObj = dict(
        short="{}.{}.{}".format(version["major"], version["minor"], version["build"]),
        long="unset",
        deb="unset",
        lobbs_short="unset",
        lobbs_long="unset",
    )

    if config.has_section("LOBBS"):
        lobbs = dict(config.items("LOBBS"))
        verObj["lobbs_short"] = "{}.{}.{}".format(lobbs["major"], lobbs["minor"], lobbs["build"])

    # Try to find current build SHA if if the workspace is clean.  This could fail if git is not installed
    try:
        # Pin abbreviation length to keep local builds and CI matching (avoid auto-shortening)
        sha = (
            subprocess.check_output(["git", "rev-parse", "--short=7", "HEAD"])
            .decode("utf-8")
            .strip()
        )
        isDirty = (
            subprocess.check_output(["git", "diff", "HEAD"]).decode("utf-8").strip()
        )
        suffix = sha
        # if isDirty:
        #     # short for 'dirty', we want to keep our verstrings source for protobuf reasons
        #     suffix = sha + "-d"
        verObj["long"] = "{}.{}".format(verObj["short"], suffix)
        verObj["deb"] = "{}.{}~{}{}".format(verObj["short"], run_number, build_location, sha)
        if verObj["lobbs_short"] != "unset":
            verObj["lobbs_long"] = "{}.{}".format(verObj["lobbs_short"], suffix)
    except:
        # print("Unexpected error:", sys.exc_info()[0])
        # traceback.print_exc()
        verObj["long"] = verObj["short"]
        verObj["deb"] = "{}.{}~{}".format(verObj["short"], run_number, build_location)
        if verObj["lobbs_short"] != "unset":
            verObj["lobbs_long"] = verObj["lobbs_short"]

    # print("firmware version " + verStr)
    return verObj


# print("path is" + ','.join(sys.path))
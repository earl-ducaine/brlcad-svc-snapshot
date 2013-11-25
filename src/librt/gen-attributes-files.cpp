// see db5_attrs.cpp (and raytrace.h) for registered attribute info

#include "common.h"

#include "bu.h"
#include "raytrace.h"
#include "db5_attrs_private.h"

#include <cstdlib>
#include <cctype>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <set>
#include <map>
#include <sys/stat.h>

using namespace std;

using namespace db5_attrs_private;

// local funcs
static void gen_attr_xml_table(const std::string& f,
                               const std::string& i,
                               const int t,
                               bool& has_registered_attrs);
static void gen_attr_xml_list(const std::string& f,
                              const std::string& i,
                              const int t);
static void open_file_write(ofstream& fs, const string& f);
static void gen_attr_html_page(const std::string& f);

// local vars
static bool debug(true);

int
main()
{

    load_maps();

    // write the html file (in librt local for now)
    // FIXME: determine desired location of the html file:
    string ofil("brlcad-attributes.html");
    gen_attr_html_page(ofil);

    // write the xml include files 'attributes.xml'
    string adir("@CMAKE_SOURCE_DIR@/doc/docbook/system/man5/en/");

    // these three files are included in the manually generated
    // 'attributes.xml' file:
    string stable(adir + "attr-std-tab-inc.xml");
    string utable(adir + "attr-usr-tab-inc.xml");
    string slist(adir + "attr-std-list-inc.xml");

    // each file needs a unique id
    string stid("std_attr_tbl");
    string utid("usr_attr_tbl");
    string slid("std_attr_info");

    bool has_registered_attrs(false);
    gen_attr_xml_table(stable, stid, ATTR_STANDARD, has_registered_attrs);
    gen_attr_xml_table(utable, utid, ATTR_REGISTERED, has_registered_attrs);
    gen_attr_xml_list(slist, slid, ATTR_STANDARD);

    return 0;
}

void
open_file_write(ofstream& fs, const string& f)
{
  fs.open(f.c_str());
  if (fs.bad()) {
    fs.close();
    bu_exit(1, "gen-attributes-files: File '%s' open error.\n", f.c_str());
  }
} // open_file_write


void
gen_attr_xml_list(const std::string& fname,
                   const std::string& id,
                   const int typ)
{
    if (typ == ATTR_REGISTERED) {
        return;
    }

    ofstream fo;
    open_file_write(fo, fname);

    // the table files will be included in a parent DocBook xml file
    // for man pages and will be child elements of a DB <para>

    string title("Standard (Core) Attributes List");

    fo <<
        "<article xmlns='http://docbook.org/ns/docbook' version='5.0'\n"
        "  xmlns:xi='http://www.w3.org/2001/XInclude'\n"
        ">\n"

        "  <info><title>" << title << "</title></info>\n"

        "  <para xml:id='" << id << "'>\n"
        "    <variablelist remap='TP'>\n"
        ;

    // watch for an empty list
    bool list_written(false);
    for (map<int,db5_attr_t>::iterator i = int2attr.begin();
         i != int2attr.end(); ++i) {
        db5_attr_t& a(i->second);
        if (a.attr_subtype != typ
            || a.long_description.empty()) {
            continue;
        }

        if (!list_written)
            list_written = true;
        fo <<
            "       <varlistentry>\n"
            "	      <term><emphasis remap='B' role='bold'>" << a.name << ":</emphasis></term>\n"
            "	      <listitem>\n"
            "	        <para>" << a.long_description << "</para>\n"
            "	      </listitem>\n"
            "       </varlistentry>\n"
            ;
    }

    fo <<
        "    </variablelist>\n"
        "  </para>\n"
        "</article>\n"
        ;

    if (!list_written) {
        bu_exit(1, "gen-attributes-files: Empty list!\n");
    }

    if (debug)
        printf("DEBUG:  see output file '%s'\n", fname.c_str());

} // gen_attr_xml_list

void
gen_attr_xml_table(const std::string& fname,
                   const std::string& id,
                   const int typ,
                   bool& has_registered_attrs)
{
    ofstream fo;
    open_file_write(fo, fname);

    // the table files will be included in a parent DocBook xml file
    // for man pages and will be child elements of a DB <para>

    string title;
    if (typ == ATTR_STANDARD) {
        title = "Standard (Core) Attributes";
    }
    else {
        title = "User-Registered Attributes";
    }

    fo <<
        "<article xmlns='http://docbook.org/ns/docbook' version='5.0'\n"
        "  xmlns:xi='http://www.w3.org/2001/XInclude'\n"
        ">\n"

        "  <info><title>" << title << "</title></info>\n"

        "  <para xml:id='" << id << "'>\n"
        ;

    if (typ == ATTR_REGISTERED && !has_registered_attrs) {

        fo <<
            "    Note:  There are no user-resistered attributes at this time.\n"
            "  </para>\n"
            "</article>\n"
            ;

        if (debug)
            printf("DEBUG:  see output file '%s'\n", fname.c_str());

        return;
    }

    fo <<
        "  <table><title>" << title << "</title>\n"
        "    <tgroup cols='6'>\n"
        "      <tbody>\n"
        "        <row>\n"
        "          <entry>Property</entry>\n"
        "          <entry>Attribute</entry>\n"
        "          <entry>Binary?</entry>\n"
        "          <entry>Definition</entry>\n"
        "          <entry>Example</entry>\n"
        "          <entry>Aliases</entry>\n"
        "        </row>\n"
        ;

    for (map<int,db5_attr_t>::iterator i = int2attr.begin();
         i != int2attr.end(); ++i) {
        db5_attr_t& a(i->second);
        if (a.attr_subtype != typ) {
            continue;
        }
        fo <<
            "        <row>\n"
            "          <entry>" << a.property                 << "</entry>\n"
            "          <entry>" << a.name                     << "</entry>\n"
            "          <entry>" << (a.is_binary ? "yes" : "") << "</entry>\n"
            "          <entry>" << a.description              << "</entry>\n"
            "          <entry>" << a.examples                 << "</entry>\n"
            "          <entry>"
            ;
        if (!a.aliases.empty()) {
            for (set<string>::iterator j = a.aliases.begin();
                 j != a.aliases.end(); ++j) {
                if (j != a.aliases.begin())
                    fo << ", ";
                fo << *j;
            }
        }
        fo <<
            "</entry>\n"
            "        </row>\n"
            ;
    }

    fo <<
        "      </tbody>\n"
        "    </tgroup>\n"
        "  </table>\n"
        "  </para>\n"
        "</article>\n"
        ;

    if (debug)
        printf("DEBUG:  see output file '%s'\n", fname.c_str());

} // gen_attr_xml_table

void
gen_attr_html_page(const std::string& fname)
{
    ofstream fo;
    open_file_write(fo, fname);

    fo <<
        "<!doctype html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "  <title>brlcad-attributes.html</title>\n"
        "  <meta charset = \"UTF-8\" />\n"
        "  <style type = \"text/css\">\n"
        "  table, td, th {\n"
        "    border: 1px solid black;\n"
        "  }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <h1>BRL-CAD Standard and User-Registered Attributes</h2>\n"
        "  <p>Following are lists of the BRL-CAD standard and user-registered attribute names\n"
        "  along with their value definitions and aliases (if any).  Users should\n"
        "  not assign values to them in other than their defined format.\n"
        "  (Note that attribute names are not case-sensitive although their canonical form is\n"
        "  lower-case.)</p>\n"

        "  <p>Any code setting or reading the value of one of these attributes\n"
        "  must handle all aliases to ensure all functions asking for\n"
        "  the value in question get a consistent answer.</p>\n"

        "  <p>Some attributes have ASCII names but binary values (e.g., 'mtime').  Their values cannot\n"
        "  be modified by a user with the 'attr' command.  In some cases, but not all, their\n"
        "  values may be shown in a human readable form with the 'attr' command.)</p>\n"

        "  <p>If a user wishes to register an attribute to protect its use for models\n"
        "  transferred to other BRL-CAD users, submit the attribute, along with a description\n"
        "  of its intended use, to the\n"
        "  <a href=\"mailto:brlcad-devel@lists.sourceforge.net\">BRL-CAD developers</a>.\n"
        "  Its approval will be formal when it appears in the separate, registered-attribute\n"
        "  table following the standard attribute table.</p>\n"
        ;

    // need a table here (6 columns at the moment)
    fo <<
        "  <h3>Standard (Core) Attributes</h3>\n"
        "  <table>\n"
        "    <tr>\n"
        "      <th>Property</th>\n"
        "      <th>Attribute</th>\n"
        "      <th>Binary?</th>\n"
        "      <th>Definition</th>\n"
        "      <th>Example</th>\n"
        "      <th>Aliases</th>\n"
        "    </tr>\n"
        ;

    // track ATTR_REGISTERED type for separate listing
    map<int,db5_attr_t> rattrs;
    for (map<int,db5_attr_t>::iterator i = int2attr.begin();
         i != int2attr.end(); ++i) {
        db5_attr_t& a(i->second);
        if (a.attr_subtype == ATTR_REGISTERED) {
            rattrs.insert(make_pair(i->first,a));
            continue;
        }
        fo <<
            "    <tr>\n"
            "      <td>" << a.property                 << "</td>\n"
            "      <td>" << a.name                     << "</td>\n"
            "      <td>" << (a.is_binary ? "yes" : "") << "</td>\n"
            "      <td>" << a.description              << "</td>\n"
            "      <td>" << a.examples                 << "</td>\n"
            "      <td>"
            ;
        if (!a.aliases.empty()) {
            for (set<string>::iterator j = a.aliases.begin();
                 j != a.aliases.end(); ++j) {
                if (j != a.aliases.begin())
                    fo << ", ";
                fo << *j;
            }
        }
        fo <<
            "</td>\n"
            "    </tr>\n"
            ;
    }
    fo << "  </table>\n";

    // now show ATTR_REGISTERED types, if any
    fo << "  <h3>User-Registered Attributes</h3>\n";
    if (rattrs.empty()) {
        fo << "    <p>None at this time.</p>\n";
    } else {
        // need a table here
        fo <<
            "  <table>\n"
            "    <tr>\n"
            "      <th>Property</th>\n"
            "      <th>Attribute</th>\n"
            "      <th>Binary?</th>\n"
            "      <th>Definition</th>\n"
            "      <th>Example</th>\n"
            "      <th>Aliases</th>\n"
            "    </tr>\n"
            ;
        for (map<int,db5_attr_t>::iterator i = rattrs.begin(); i != rattrs.end(); ++i) {
            db5_attr_t& a(i->second);
            fo <<
                "    <tr>\n"
                "      <td>" << a.property                 << "</td>\n"
                "      <td>" << a.name                     << "</td>\n"
                "      <td>" << (a.is_binary ? "yes" : "") << "</td>\n"
                "      <td>" << a.description              << "</td>\n"
                "      <td>" << a.examples                 << "</td>\n"
                "      <td>"
                ;
            if (!a.aliases.empty()) {
                for (set<string>::iterator j = a.aliases.begin();
                     j != a.aliases.end(); ++i) {
                    if (j != a.aliases.begin())
                        fo << ", ";
                    fo << *j;
                }
            }
            fo <<
                "</td>\n"
                "    </tr>\n"
                ;
        }
        fo << "  </table>\n";
    }

    fo <<
        "</body>\n"
        "</html>\n"
        ;

    fo.close();

    if (debug)
        printf("DEBUG:  see output file '%s'\n", fname.c_str());

} // gen_attr_html_page

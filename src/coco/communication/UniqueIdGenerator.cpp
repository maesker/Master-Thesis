#include <locale>
#include <sstream>
#include <iomanip>
#include <locale>

// To read mac address
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

#include <string.h> /* for strncpy */

#include "coco/communication/UniqueIdGenerator.h"

#include "pc2fsProfiler/Pc2fsProfiler.h"

#define SEPERATOR "#"


UniqueIdGenerator::UniqueIdGenerator()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    last_id = 0;
    determine_uid();

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();
}

UniqueIdGenerator::~UniqueIdGenerator()
{
}


/**
 * Get the next id.
 * The id the generating by using an integer value + the mac address of the machine.
 * The string looks like following: ID#mac-address.
 * Finally, an hash function is applied on this string.
 * @return Returns an unique id.
 */
uint64_t UniqueIdGenerator::get_next_id()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    string str_id;
    string str_temp;
    ostringstream ostr;
    locale loc;
    const collate<char>& coll = use_facet<collate<char> >(loc);
    uint64_t hash;


    last_id++;

    // convert last_id to string
    ostr << last_id;
    str_id = ostr.str();

    str_id += SEPERATOR + uid;

    /*
     * generating a hash from string
     * @see http://www.cplusplus.com/reference/std/locale/collate/hash/
     */
    hash = coll.hash(str_id.data(),str_id.data()+str_id.length());

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return hash;
}

/**
 * Determines the unique id.
 * The id is based on the MAC-address.
 * @returns The unique id as a string.
 */
string UniqueIdGenerator::determine_uid()
{
    // starts profiling
    Pc2fsProfiler::get_instance()->function_start();

    ostringstream ostr;
    string uid;
    /**
     * Get the mac address
     */
    int fd;
    struct ifreq ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);

    ioctl(fd, SIOCGIFHWADDR, &ifr);

    //close(fd);
    shutdown(fd, 0);
    /***********************************************/


    // Convert the mac address from byte representation to string representation
    for (int i=0; i<6; i++)
    {
        ostr << hex << setfill('0') << setw(2)
        << static_cast<unsigned int>(static_cast<unsigned char>(ifr.ifr_hwaddr.sa_data[i]));
    }

    uid = ostr.str();

    // ends profiling
    Pc2fsProfiler::get_instance()->function_end();

    return uid;
}

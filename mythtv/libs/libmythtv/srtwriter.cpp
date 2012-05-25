#include "srtwriter.h"

// SRTWriter implementation

#ifdef SRT_TO_ASCII
static QString to_ascii(const QString &str)
{
    QString ret = "";

    for (int i = 0; i < str.length(); i++)
    {
        QChar cp = str[i];
        int cpu = cp.unicode();
        switch (cpu)
        {
            case '\b':  ret +=
                (ret.length()) ? ret.left(ret.length()-1) : ret; break;
                break;
            case 0xA0:
                ret += " "; break;
            case 0xA1:
                ret += "!"; break;
            case 0xA2:
                ret += "c"; break;
            case 0xA3:
                ret += "lb"; break;
            case 0xA4:
                ret += "o"; break;
            case 0xA5:
                ret += "Y"; break;
            case 0xA6:
                ret += "|"; break;
            case 0xA7:
                ret += "SS"; break;
            case 0xA8:
                ret += ".."; break;
            case 0xA9:
                ret += "(c)"; break;
            case 0xAA:
                ret += "a"; break;
            case 0xAB:
                ret += "<<"; break;
            case 0xAC:
                ret += "-|"; break;
            case 0xAD:
                ret += "SHY"; break;
            case 0xAE:
                ret += "(R)"; break;
            case 0xAF:
                ret += "-"; break;
            case 0xB1:
                ret += "+/-"; break;
            case 0xB2:
                ret += "^2"; break;
            case 0xB3:
                ret += "^3"; break;
            case 0xB4:
                ret += "'"; break;
            case 0xB5:
                ret += "U"; break;
            case 0xB6:
                ret += "P"; break;
            case 0xB7:
                ret += "*"; break;
            case 0xB8:
                ret += "_|"; break;
            case 0xB9:
                ret += "^1"; break;
            case 0xBA:
                ret += "o"; break;
            case 0xBB:
                ret += ">>";
            case 0xBC:
                ret += "1/4"; break;
            case 0xBD:
                ret += "1/2"; break;
            case 0xBE:
                ret += "3/4"; break;
            case 0xBF:
                ret += "?"; break;

            case 0xC0: case 0xC1: case 0xC2: case 0xC3:
            case 0xC4: case 0xC5:
                ret += "A"; break;
            case 0xC6:
                ret += "AE"; break;
            case 0xC7:
                ret += "C"; break;
            case 0xC8: case 0xC9: case 0xCA: case 0xCB:
                ret += "E"; break;
            case 0xCC: case 0xCD: case 0xCE: case 0xCF:
                ret += "I"; break;
            case 0xD0:
                ret += "D"; break;
            case 0xD1:
                ret += "N"; break;
            case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6:
                ret += "O"; break;
            case 0xD7:
                ret += "*"; break;
            case 0xD8:
                ret += "O"; break;
            case 0xD9: case 0xDA: case 0xDB: case 0xDC:
                ret += "U"; break;
            case 0xDD:
                ret += "Y"; break;
            case 0xDE:
                ret += "TH"; break;
            case 0xDF:
                ret += "B"; break;
            case 0xE0: case 0xE1: case 0xE2: case 0xE3:
            case 0xE4: case 0xE5:
                ret += "a"; break;
            case 0xE6: ret += "ae"; break;
            case 0xE7: ret += "c"; break;
            case 0xE8: case 0xE9: case 0xEA: case 0xEB:
                ret += "e"; break;
            case 0xEC: case 0xED: case 0xEE: case 0xEF:
                ret += "i"; break;
            case 0xF0: ret += "o"; break;
            case 0xF1: ret += "n"; break;
            case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6:
                ret += "o"; break;
            case 0xF7: ret += "/"; break;
            case 0xF8: ret += "o"; break;
            case 0xF9: case 0xFA: case 0xFB: case 0xFC:
                ret += "u"; break;
            case 0xFD: ret += "y"; break;
            case 0xFE: ret += "th"; break;
            case 0xFF: ret += "y"; break;

            case 0x2120 :  ret += "(SM)"; break;
            case 0x2122 :  ret += "(TM)"; break;
            case 0x2014 :  ret += "(--)"; break;
            case 0x201C :  ret += "``"; break;
            case 0x201D :  ret += "''"; break;
            case 0x250C :  ret += "|-"; break;
            case 0x2510 :  ret += "-|"; break;
            case 0x2514 :  ret += "|_"; break;
            case 0x2518 :  ret += "_|"; break;
            case 0x2588 :  ret += "[]"; break;
            case 0x266A :  ret += "o"; break; // music note

            default:
                if (cpu < 0xA0)
                    ret += QString(cp.toLatin1());
        }
    }

    return ret;
}
#endif // SRT_TO_ASCII

/**
 * Adds next subtitle.
 */
void SRTWriter::AddSubtitle(const OneSubtitle &sub, int number)
{
    m_outStream << number << endl;

    m_outStream << FormatTime(sub.start_time) << " --> ";
    m_outStream << FormatTime(sub.start_time + sub.length) << endl;

    if (!sub.text.isEmpty())
    {
        QStringList::const_iterator it = sub.text.begin();
        for (; it != sub.text.end(); ++it)
        {
#ifdef SRT_TO_ASCII
            m_outStream << to_ascii(*it) << endl;
#else
            m_outStream << *it << endl;
#endif
        }
        m_outStream << endl;
    }
}

/**
 * Formats time to format appropriate to SubRip file.
 */
QString SRTWriter::FormatTime(uint64_t time_in_msec)
{
    uint64_t msec = time_in_msec % 1000;
    time_in_msec /= 1000;

    uint64_t ss = time_in_msec % 60;
    time_in_msec /= 60;

    uint64_t mm = time_in_msec % 60;
    time_in_msec /= 60;

    uint64_t hh = time_in_msec;

    return QString("%1:%2:%3,%4")
        .arg(hh,2,10,QChar('0'))
        .arg(mm,2,10,QChar('0'))
        .arg(ss,2,10,QChar('0'))
        .arg(msec,3,10,QChar('0'));
}

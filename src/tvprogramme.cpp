/*
 * Copyright (C) 2011  Southern Storm Software, Pty Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "tvprogramme.h"
#include "tvchannel.h"
#include "tvchannellist.h"
#include <QtCore/qdebug.h>
#include <QtGui/qtextdocument.h>
#include <QtGui/qtextlist.h>
#include <QtGui/qtexttable.h>

TvProgramme::TvProgramme(TvChannel *channel)
    : m_channel(channel)
    , m_season(0)
    , m_isPremiere(false)
    , m_isRepeat(false)
    , m_isMovie(false)
    , m_suppressed(false)
    , m_shortDescriptionDocument(0)
    , m_bookmark(0)
    , m_match(TvBookmark::NoMatch)
    , m_prev(0)
    , m_next(0)
{
}

TvProgramme::~TvProgramme()
{
    if (m_bookmark)
        m_bookmark->removeProgramme(this);
    delete m_shortDescriptionDocument;
}

int TvProgramme::year() const
{
    bool ok = false;
    int yr = m_date.toInt(&ok);
    if (ok && yr >= 1900)
        return yr;
    else
        return 0;
}

// Episode numbers in the "xmltv_ns" system are of the form
// A.B.C, where each of the numbers is 0-based, not 1-based.
// The numbers could also have the form X/Y for multiple parts.
// Fix the number so it is closer to what the user expects.
static QString fixEpisodeNumber(const QString &str, int *season)
{
    QStringList components = str.split(QLatin1String("."));
    QString result;
    bool needDot = false;
    *season = 0;
    for (int index = 0; index < components.size(); ++index) {
        QString comp = components.at(index);
        int slash = comp.indexOf(QLatin1Char('/'));
        if (slash >= 0)
            comp = comp.left(slash);
        if (comp.isEmpty())
            continue;
        if (needDot) {
            if (index == 2)
                result += QObject::tr(", Part ");
            else
                result += QLatin1Char('.');
        }
        result += QString::number(comp.toInt() + 1);
        if (!needDot)
            *season = comp.toInt() + 1;
        needDot = true;
    }
    return result;
}

void TvProgramme::load(QXmlStreamReader *reader)
{
    // Will leave the XML stream positioned on </programme>.
    // Format details:
    //    http://xmltv.cvs.sourceforge.net/viewvc/xmltv/xmltv/xmltv.dtd
    // The parser is not yet complete - lots of other information
    // may be provided.  This set is reasonably inclusive of the
    // data found in OzTivo listings.  Feel free to add extra fields
    // if you see the warning printed.
    Q_ASSERT(reader->isStartElement());
    Q_ASSERT(reader->name() == QLatin1String("programme"));
    QString start = reader->attributes().value(QLatin1String("start")).toString();
    QString stop = reader->attributes().value(QLatin1String("stop")).toString();
    m_start = TvChannel::stringToDateTime(start, m_channel);
    m_stop = TvChannel::stringToDateTime(stop, m_channel);
    while (!reader->hasError()) {
        QXmlStreamReader::TokenType token = reader->readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (reader->name() == QLatin1String("title")) {
                m_title = reader->readElementText
                    (QXmlStreamReader::IncludeChildElements);
                m_indexTitle = m_title.toLower();
            } else if (reader->name() == QLatin1String("sub-title")) {
                m_subTitle = reader->readElementText
                    (QXmlStreamReader::IncludeChildElements);
            } else if (reader->name() == QLatin1String("desc")) {
                m_description = reader->readElementText
                    (QXmlStreamReader::IncludeChildElements);
            } else if (reader->name() == QLatin1String("date")) {
                m_date = reader->readElementText
                    (QXmlStreamReader::IncludeChildElements);
            } else if (reader->name() == QLatin1String("director")) {
                m_directors += reader->readElementText
                    (QXmlStreamReader::IncludeChildElements);
            } else if (reader->name() == QLatin1String("actor")) {
                m_actors += reader->readElementText
                    (QXmlStreamReader::IncludeChildElements);
            } else if (reader->name() == QLatin1String("presenter")) {
                m_presenters += reader->readElementText
                    (QXmlStreamReader::IncludeChildElements);
            } else if (reader->name() == QLatin1String("guest")) {
                addOtherCredit
                    (QObject::tr("Guest"),
                     reader->readElementText
                        (QXmlStreamReader::IncludeChildElements));
            } else if (reader->name() == QLatin1String("commentator")) {
                addOtherCredit
                    (QObject::tr("Commentator"),
                     reader->readElementText
                        (QXmlStreamReader::IncludeChildElements));
            } else if (reader->name() == QLatin1String("writer")) {
                addOtherCredit
                    (QObject::tr("Writer"),
                     reader->readElementText
                        (QXmlStreamReader::IncludeChildElements));
            } else if (reader->name() == QLatin1String("adapter")) {
                addOtherCredit
                    (QObject::tr("Adapted By"),
                     reader->readElementText
                        (QXmlStreamReader::IncludeChildElements));
            } else if (reader->name() == QLatin1String("producer")) {
                addOtherCredit
                    (QObject::tr("Producer"),
                     reader->readElementText
                        (QXmlStreamReader::IncludeChildElements));
            } else if (reader->name() == QLatin1String("composer")) {
                addOtherCredit
                    (QObject::tr("Composer"),
                     reader->readElementText
                        (QXmlStreamReader::IncludeChildElements));
            } else if (reader->name() == QLatin1String("editor")) {
                addOtherCredit
                    (QObject::tr("Editor"),
                     reader->readElementText
                        (QXmlStreamReader::IncludeChildElements));
            } else if (reader->name() == QLatin1String("category")) {
                QString category = reader->readElementText
                    (QXmlStreamReader::IncludeChildElements);
                m_categories += category;
                if (category == QLatin1String("Movie") ||
                        category == QLatin1String("Movies"))
                    m_isMovie = true;   // FIXME: other languages
            } else if (reader->name() == QLatin1String("rating")) {
                m_rating = reader->readElementText
                    (QXmlStreamReader::IncludeChildElements);
            } else if (reader->name() == QLatin1String("star-rating")) {
                m_starRating = reader->readElementText
                    (QXmlStreamReader::IncludeChildElements);
            } else if (reader->name() == QLatin1String("episode-num")) {
                QStringRef system = reader->attributes().value(QLatin1String("system"));
                if (system == QLatin1String("xmltv_ns")) {
                    m_episodeNumber = fixEpisodeNumber
                        (reader->readElementText
                            (QXmlStreamReader::IncludeChildElements),
                         &m_season);
                }
            } else if (reader->name() == QLatin1String("language")) {
                m_language = reader->readElementText
                    (QXmlStreamReader::IncludeChildElements);
            } else if (reader->name() == QLatin1String("orig-language")) {
                m_originalLanguage = reader->readElementText
                    (QXmlStreamReader::IncludeChildElements);
            } else if (reader->name() == QLatin1String("country")) {
                m_country = reader->readElementText
                    (QXmlStreamReader::IncludeChildElements);
            } else if (reader->name() == QLatin1String("aspect")) {
                m_aspectRatio = reader->readElementText
                    (QXmlStreamReader::IncludeChildElements);
            } else if (reader->name() == QLatin1String("premiere")) {
                m_isPremiere = true;
            } else if (reader->name() == QLatin1String("previously-shown")) {
                m_isRepeat = true;
            // The following are container elements that are ignored.
            } else if (reader->name() == QLatin1String("credits") ||
                       reader->name() == QLatin1String("video")) {
            // The following are in the DTD, but not processed yet.
            } else if (reader->name() == QLatin1String("present") ||
                       reader->name() == QLatin1String("quality") ||
                       reader->name() == QLatin1String("audio") ||
                       reader->name() == QLatin1String("stereo") ||
                       reader->name() == QLatin1String("last-chance") ||
                       reader->name() == QLatin1String("new") ||
                       reader->name() == QLatin1String("subtitles") ||
                       reader->name() == QLatin1String("review") ||
                       reader->name() == QLatin1String("url") ||
                       reader->name() == QLatin1String("length") ||
                       reader->name() == QLatin1String("icon")) {
                qWarning() << "Warning: unhandled standard programme element:" << reader->name();
            } else {
                qWarning() << "Warning: unknown programme element:" << reader->name();
            }
        } else if (token == QXmlStreamReader::EndElement) {
            if (reader->name() == QLatin1String("programme"))
                break;
        }
    }
    delete m_shortDescriptionDocument;
    m_shortDescriptionDocument = 0;
    m_longDescription = QString();

    // Find the bookmark that best matches the programme.
    if (!m_channel->m_needBookmarkRefresh) {
        TvBookmark *bookmark = 0;
        TvBookmark::Match match =
            m_channel->channelList()->bookmarkList()->match
                (this, &bookmark, TvBookmark::PartialMatches | TvBookmark::NonMatching);
        setBookmark(bookmark, match);
    }
}

void TvProgramme::setBookmark
    (TvBookmark *bookmark, TvBookmark::Match match)
{
    if (m_bookmark) {
        if (m_bookmark != bookmark) {
            m_bookmark->removeProgramme(this);
            if (bookmark)
                bookmark->addProgramme(this);
            m_bookmark = bookmark;
        }
    } else {
        m_bookmark = bookmark;
        if (bookmark)
            bookmark->addProgramme(this);
    }
    if (m_match != match) {
        m_match = match;
        delete m_shortDescriptionDocument;
        m_shortDescriptionDocument = 0;
    }
}

void TvProgramme::clearBookmarkMatch()
{
    m_bookmark = 0;
    m_match = TvBookmark::NoMatch;
    delete m_shortDescriptionDocument;
    m_shortDescriptionDocument = 0;
}

void TvProgramme::markDirty()
{
    delete m_shortDescriptionDocument;
    m_shortDescriptionDocument = 0;
}

static bool sortMovedProgrammes(TvProgramme *p1, TvProgramme *p2)
{
    int cmp = p1->channel()->compare(p2->channel());
    if (cmp != 0)
        return cmp < 0;
    return p1->start() < p2->start();
}

static QList<TvProgramme *> reduceMovedToList(const QList<TvProgramme *> &list)
{
    QList<TvProgramme *> result;
    for (int index = 0; index < list.size(); ++index)
        list.at(index)->setSuppressed(false);
    for (int index = 0; index < list.size(); ++index) {
        TvProgramme *prog = list.at(index);
        if (prog->isSuppressed() || prog->subTitle().isEmpty())
            continue;
        for (int index2 = index + 1; index2 < list.size(); ++index2) {
            TvProgramme *prog2 = list.at(index2);
            if (prog2->isSuppressed())
                continue;
            if (prog->subTitle() == prog2->subTitle())
                prog2->setSuppressed(true);
        }
        result.append(prog);
    }
    return result;
}

QTextDocument *TvProgramme::shortDescriptionDocument() const
{
    if (m_shortDescriptionDocument)
        return m_shortDescriptionDocument;
    m_shortDescriptionDocument = new QTextDocument();
    QTextCursor cursor(m_shortDescriptionDocument);
    writeShortDescription(&cursor, Write_Short);
    return m_shortDescriptionDocument;
}

QString TvProgramme::longDescription() const
{
    if (!m_longDescription.isEmpty())
        return m_longDescription;
    QString desc = QLatin1String("<qt><p><i>");
    desc += Qt::escape(m_description) + QLatin1String("</i></p>");
    desc += QObject::tr("<p><b>Duration:</b> %1 minutes, %2 to %3</p>")
                .arg(QString::number(m_start.secsTo(m_stop) / 60))
                .arg(m_start.time().toString(Qt::LocaleDate))
                .arg(m_stop.time().toString(Qt::LocaleDate));
    if (m_categories.size() > 1) {
        desc += QObject::tr("<p><b>Categories:</b> %1</p>")
            .arg(Qt::escape(m_categories.join(QLatin1String(", "))));
    }
    if (!m_actors.isEmpty()) {
        desc += QObject::tr("<p><b>Starring:</b> %1</p>")
            .arg(Qt::escape(m_actors.join(QLatin1String(", "))));
    }
    if (!m_presenters.isEmpty()) {
        desc += QObject::tr("<p><b>Presenter:</b> %1</p>")
            .arg(Qt::escape(m_presenters.join(QLatin1String(", "))));
    }
    if (!m_otherCredits.isEmpty()) {
        QMap<QString, QStringList>::ConstIterator it;
        for (it = m_otherCredits.constBegin();
                it != m_otherCredits.constEnd(); ++it) {
            desc += QString(QLatin1String("<p><b>%1:</b> %2</p>"))
                .arg(Qt::escape(it.key()))
                .arg(Qt::escape(it.value().join(QLatin1String(", "))));
        }
    }
    if (!m_directors.isEmpty()) {
        desc += QObject::tr("<p><b>Director:</b> %1</p>")
            .arg(Qt::escape(m_directors.join(QLatin1String(", "))));
    }
    if (!m_language.isEmpty() && !m_originalLanguage.isEmpty() &&
            m_language != m_originalLanguage) {
        desc += QObject::tr("<p><b>Language:</b> %1 (original in %2)</p>")
            .arg(Qt::escape(m_language), Qt::escape(m_originalLanguage));
    } else if (!m_language.isEmpty()) {
        desc += QObject::tr("<p><b>Language:</b> %1</p>")
            .arg(Qt::escape(m_language));
    } else if (!m_originalLanguage.isEmpty()) {
        desc += QObject::tr("<p><b>Original Language:</b> %1</p>")
            .arg(Qt::escape(m_originalLanguage));
    }
    if (!m_country.isEmpty()) {
        desc += QObject::tr("<p><b>Country:</b> %1</p>")
            .arg(Qt::escape(m_country));
    }
    if (!m_aspectRatio.isEmpty()) {
        desc += QObject::tr("<p><b>Aspect ratio:</b> %1</p>")
            .arg(Qt::escape(m_aspectRatio));
    }
    desc += QLatin1String("</qt>");
    m_longDescription = desc;
    return desc;
}

void TvProgramme::writeShortDescription(QTextCursor *cursor, int options) const
{
    QTextCharFormat prevFormat = cursor->charFormat();

    TvBookmark::Match match = displayMatch();
    QTextCharFormat titleFormat = prevFormat;
    switch (match) {
    case TvBookmark::NoMatch:
    case TvBookmark::ShouldMatch:
    case TvBookmark::TickMatch:
        break;
    case TvBookmark::FullMatch:
        titleFormat.setForeground(m_bookmark->color());
        titleFormat.setFontWeight(QFont::Bold);
        break;
    case TvBookmark::Overrun:
    case TvBookmark::Underrun:
        titleFormat.setForeground(m_bookmark->color().lighter(150));
        titleFormat.setFontWeight(QFont::Bold);
        break;
    case TvBookmark::TitleMatch:
        titleFormat.setForeground(m_bookmark->color());
        break;
    }
    if (m_isMovie)
        cursor->insertText(QObject::tr("MOVIE: "), titleFormat);
    cursor->insertText(m_title, titleFormat);
    if (options & Write_Continued)
        cursor->insertText(QObject::tr(" (cont)"));

    QTextCharFormat detailsFormat = prevFormat;
    QTextCharFormat italicDetailsFormat;
    detailsFormat.setForeground(QColor::fromRgb(0x60, 0x60, 0x60));
    italicDetailsFormat = detailsFormat;
    italicDetailsFormat.setFontItalic(true);
    cursor->setCharFormat(detailsFormat);

    QTextBlockFormat mainBlockFormat = cursor->blockFormat();

    if (!m_date.isEmpty() && m_date != QLatin1String("0") &&
                (options & Write_Date) != 0) {
        cursor->insertText(QLatin1String(" (") +
                           m_date + QLatin1String(")"));
    }
    if (!m_rating.isEmpty()) {
        cursor->insertText(QLatin1String(" (") +
                           m_rating + QLatin1String(")"));
        if (m_isRepeat)
            cursor->insertText(QObject::tr("(R)"));
    } else if (m_isRepeat) {
        cursor->insertText(QObject::tr(" (R)"));
    }
    if (options & Write_Category) {
        if (!m_categories.isEmpty()) {
            QString category = m_categories.at(0);
            for (int index = 1; index < m_categories.size(); ++index) {
                if (category.compare(QLatin1String("series")) != 0 &&
                        category.compare(QLatin1String("movies")) != 0 &&
                        category.compare(QLatin1String("movie")) != 0)
                    break;
                category = m_categories.at(index);
            }
            cursor->insertText(QLatin1String(", ") + category);
        }
    }
    if (options & Write_Actor) {
        if (!m_actors.isEmpty()) {
            cursor->insertText(QLatin1String(", ") + m_actors.at(0));
        } else if (!m_presenters.isEmpty()) {
            cursor->insertText(QLatin1String(", ") + m_presenters.at(0));
        } else if (!m_directors.isEmpty()) {
            cursor->insertText
                (QLatin1String(", ") +
                QObject::tr("Director: %1").arg(m_directors.at(0)));
        }
    }
    if (!m_starRating.isEmpty() && (options & Write_StarRating) != 0) {
        QStringList list = m_starRating.split(QLatin1String("/"));
        if (list.size() >= 2) {
            int numer = list.at(0).toInt();
            int denom = list.at(1).toInt();
            if (numer > 0 && denom > 0) {
                qreal stars = numer * 5.0f / denom;
                if (stars < 0.0f)
                    stars = 0.0f;
                else if (stars > 5.0f)
                    stars = 5.0f;
                QString rating(QLatin1String(" "));
                int istars = int(stars);
                qreal leftOver = stars - istars;
                for (int black = 0; black < istars; ++black)
                    rating += QChar(0x2605);  // Black star.
                if (leftOver >= 0.5f) {
                    rating += QChar(0x272D);  // Black outlined star.
                    ++istars;
                }
                for (int white = istars; white < 5; ++white)
                    rating += QChar(0x2606);  // White star.
                cursor->insertText(rating);
            }
        }
    }
    if (options & Write_EpisodeName) {
        if (!m_subTitle.isEmpty()) {
            cursor->insertBlock(mainBlockFormat);
            cursor->insertText(m_subTitle, italicDetailsFormat);
            cursor->setCharFormat(detailsFormat);
        }
        if (!m_episodeNumber.isEmpty()) {
            cursor->insertText(QObject::tr(" (Episode %1)")
                                    .arg(m_episodeNumber));
        }
        if (m_isPremiere) {
            cursor->insertText(QLatin1String(", "));
            QTextCharFormat redFormat = detailsFormat;
            redFormat.setForeground(Qt::red);
            redFormat.setFontWeight(QFont::Bold);
            cursor->insertText(QObject::tr("Premiere"), redFormat);
            cursor->setCharFormat(detailsFormat);
        }
    }
    if (match == TvBookmark::ShouldMatch) {
        QString title = m_bookmark->title();
        if (!m_bookmark->seasonList().isEmpty()) {
            title = QObject::tr("%1, Season %2")
                        .arg(title).arg(m_bookmark->seasons());
        }
        if (!m_bookmark->yearList().isEmpty()) {
            title += QLatin1String(", ") + m_bookmark->years();
        }
        QTextCharFormat strikeOutFormat = detailsFormat;
        strikeOutFormat.setFontStrikeOut(true);
        cursor->insertBlock(mainBlockFormat);
        cursor->setCharFormat(strikeOutFormat);
        cursor->insertText(title);
        cursor->setCharFormat(detailsFormat);
        if (options & Write_MovedTo) {
            QList<TvProgramme *> others = m_bookmark->m_matchingProgrammes;
            QTextList *list = 0;
            qSort(others.begin(), others.end(), sortMovedProgrammes);
            others = reduceMovedToList(others);
            for (int index = 0; index < others.size(); ++index) {
                TvProgramme *other = others.at(index);
                if (other->match() != TvBookmark::TitleMatch)
                    continue;
                if (list) {
                    cursor->insertBlock();
                    list->add(cursor->block());
                } else {
                    cursor->insertText(QObject::tr(" may have moved to:"));
                    list = cursor->insertList(QTextListFormat::ListDisc);
                }
                cursor->insertText(other->channel()->name());
                cursor->insertText(other->start().date().toString(QLatin1String(" dddd, MMMM d, ")));
                cursor->insertText(other->start().time().toString(Qt::LocaleDate));
            }
        }
    }
    if (match != TvBookmark::NoMatch &&
            match != TvBookmark::ShouldMatch &&
            match != TvBookmark::TickMatch &&
            !m_subTitle.isEmpty() &&
            (options & Write_OtherShowings) != 0) {
        QList<TvProgramme *> others = m_bookmark->m_matchingProgrammes;
        qSort(others.begin(), others.end(), sortMovedProgrammes);
        bool needComma = false;
        bool explicitToday = false;
        for (int index = 0; index < others.size(); ++index) {
            TvProgramme *other = others.at(index);
            if (other == this || other->subTitle() != m_subTitle)
                continue;
            if (needComma) {
                cursor->insertText(QLatin1String(", "));
            } else {
                cursor->insertBlock(mainBlockFormat);
                cursor->insertText(QObject::tr("Other showings: "));
                needComma = true;
            }
            if (m_channel != other->channel()) {
                cursor->insertText(other->channel()->name());
                cursor->insertText(QLatin1String(" "));
            }
            if (m_start.date() != other->start().date() || explicitToday) {
                int diff = m_start.date().daysTo(other->start().date());
                if (diff >= 0 && diff <= 6) {
                    cursor->insertText(QDate::longDayName(other->start().date().dayOfWeek()));
                    cursor->insertText(QLatin1String(" "));
                } else {
                    cursor->insertText(other->start().date().toString(QLatin1String("dddd, MMMM d, ")));
                }
                explicitToday = true;
            }
            cursor->insertText(other->start().time().toString(Qt::LocaleDate));
        }
    }
    if (options & Write_Description) {
        // Add the first sentence of the description.
        QString desc = m_description;
        int index = desc.indexOf(QLatin1Char('.'));
        if (index >= 0)
            desc = desc.left(index + 1);
        if (!desc.isEmpty()) {
            cursor->insertBlock(mainBlockFormat);
            cursor->insertText(desc);
        }
    }

    cursor->setCharFormat(prevFormat);
    cursor->setBlockFormat(mainBlockFormat);
}

TvBookmark::Match TvProgramme::displayMatch() const
{
    TvBookmark::Match result = m_match;

    if (m_match == TvBookmark::ShouldMatch) {
        // Suppress failed matches either side of a successful match,
        // and remove redundant failed matches.
        if (m_prev && m_prev->bookmark() == m_bookmark) {
            if (m_prev->match() != TvBookmark::NoMatch &&
                    m_prev->match() != TvBookmark::TickMatch)
                result = TvBookmark::NoMatch;
        } else if (m_next && m_next->bookmark() == m_bookmark) {
            if (m_next->match() != TvBookmark::ShouldMatch &&
                    m_next->match() != TvBookmark::NoMatch &&
                    m_next->match() != TvBookmark::TickMatch)
                result = TvBookmark::NoMatch;
        }
    } else if (m_match == TvBookmark::TitleMatch) {
        // Partial match immediately before or after a full
        // match is labelled as an underrun or overrun.
        // Probably a double episode where one of the episodes
        // falls outside the normal bookmark range.
        if (m_prev && m_prev->stop() == m_start &&
                m_prev->match() == TvBookmark::FullMatch &&
                m_prev->bookmark() == m_bookmark) {
            result = TvBookmark::Overrun;
        } else if (m_next && m_next->start() == m_stop &&
                   m_next->match() == TvBookmark::FullMatch &&
                   m_next->bookmark() == m_bookmark) {
            result = TvBookmark::Underrun;
        }
    }

    return result;
}

void TvProgramme::refreshBookmark()
{
    TvBookmark *bookmark = 0;
    TvBookmark::Match match =
        m_channel->channelList()->bookmarkList()->match
            (this, &bookmark, TvBookmark::PartialMatches | TvBookmark::NonMatching);
    setBookmark(bookmark, match);
}

void TvProgramme::addOtherCredit(const QString &type, const QString &name)
{
    QMap<QString, QStringList>::Iterator it;
    it = m_otherCredits.find(type);
    if (it != m_otherCredits.end())
        it.value().append(name);
    else
        m_otherCredits.insert(type, QStringList(name));
}

bool TvProgramme::containsSearchString(const QString &str, SearchType type) const
{
    switch (type) {
    case SearchTitle:
        return containsSearch(str, m_title);
    case SearchEpisodeName:
        return containsSearch(str, m_subTitle);
    case SearchDescription:
        return containsSearch(str, m_description);
    case SearchAll:
        if (containsSearch(str, m_title))
            return true;
        if (containsSearch(str, m_subTitle))
            return true;
        if (containsSearch(str, m_description))
            return true;
        if (containsSearch(str, m_categories))
            return true;
        // Fall through to the next case.
    case SearchCredits: {
        if (containsSearch(str, m_directors))
            return true;
        if (containsSearch(str, m_actors))
            return true;
        if (containsSearch(str, m_presenters))
            return true;
        QMap<QString, QStringList>::ConstIterator it;
        for (it = m_otherCredits.constBegin();
                it != m_otherCredits.constEnd(); ++it) {
            if (containsSearch(str, it.value()))
                return true;
        }
        return false; }
    case SearchCategories:
        return containsSearch(str, m_categories);
    }
    return false;
}

bool TvProgramme::containsSearch(const QString &str, const QStringList &within) const
{
    for (int index = 0; index < within.size(); ++index) {
        if (containsSearch(str, within.at(index)))
            return true;
    }
    return false;
}

void TvProgramme::updateCategorySet(QSet<QString> &set) const
{
    updateSet(set, m_categories);
}

void TvProgramme::updateCreditSet(QSet<QString> &set) const
{
    updateSet(set, m_directors);
    updateSet(set, m_actors);
    updateSet(set, m_presenters);
    QMap<QString, QStringList>::ConstIterator it;
    for (it = m_otherCredits.constBegin();
            it != m_otherCredits.constEnd(); ++it) {
        updateSet(set, it.value());
    }
}

void TvProgramme::updateSet(QSet<QString> &set, const QStringList &list)
{
    for (int index = 0; index < list.size(); ++index)
        set.insert(list.at(index));
}

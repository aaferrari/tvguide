/*
 * Copyright (C) 2011,2012  Southern Storm Software, Pty Ltd.
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

#include "tvtick.h"
#include "tvprogramme.h"
#include "tvchannel.h"

TvTick::TvTick()
{
}

TvTick::~TvTick()
{
}

bool TvTick::match(const TvProgramme *programme) const
{
    if (programme->start() != m_start)
        return false;
    if (!programme->channel()->isSameChannel(m_channelId))
        return false;
    return programme->title() == m_title;
}

void TvTick::load(QSettings *settings)
{
    m_title = settings->value(QLatin1String("title")).toString();
    m_channelId = settings->value(QLatin1String("channelId")).toString();
    m_start = QDateTime::fromString(settings->value(QLatin1String("start")).toString(), Qt::ISODate);
    m_timestamp = QDateTime::fromString(settings->value(QLatin1String("timestamp")).toString(), Qt::ISODate);
}

void TvTick::save(QSettings *settings)
{
    settings->setValue(QLatin1String("title"), m_title);
    settings->setValue(QLatin1String("channelId"), m_channelId);
    settings->setValue(QLatin1String("start"), m_start.toString(Qt::ISODate));
    settings->setValue(QLatin1String("timestamp"), m_timestamp.toString(Qt::ISODate));
}

void TvTick::loadXml(QXmlStreamReader *reader)
{
    Q_ASSERT(reader->isStartElement());
    Q_ASSERT(reader->name() == QLatin1String("tick"));
    while (!reader->hasError()) {
        QXmlStreamReader::TokenType token = reader->readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (reader->name() == QLatin1String("title")) {
                m_title = reader->readElementText
                    (QXmlStreamReader::IncludeChildElements);
            } else if (reader->name() == QLatin1String("channel-id")) {
                m_channelId = reader->readElementText
                    (QXmlStreamReader::IncludeChildElements);
            } else if (reader->name() == QLatin1String("start-time")) {
                m_start = TvChannel::stringToDateTime(reader->readElementText
                    (QXmlStreamReader::IncludeChildElements));
            } else if (reader->name() == QLatin1String("timestamp")) {
                m_timestamp = TvChannel::stringToDateTime(reader->readElementText
                    (QXmlStreamReader::IncludeChildElements));
            }
        } else if (token == QXmlStreamReader::EndElement) {
            if (reader->name() == QLatin1String("tick"))
                break;
        }
    }
}

void TvTick::saveXml(QXmlStreamWriter *writer)
{
    writer->writeStartElement(QLatin1String("tick"));
    writer->writeTextElement(QLatin1String("title"), m_title);
    writer->writeTextElement(QLatin1String("channel-id"), m_channelId);
    writer->writeTextElement(QLatin1String("start-time"), m_start.toString(QLatin1String("yyyyMMddhhmmss")));
    writer->writeTextElement(QLatin1String("timestamp"), m_timestamp.toString(QLatin1String("yyyyMMddhhmmss")));
    writer->writeEndElement();
}

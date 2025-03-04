/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <QIODevice>

using ::testing::_;
using ::testing::Return;

#include "framework/network/networktypes.h"

#include "mocks/updateconfigurationmock.h"
#include "network/tests/mocks/networkmanagercreatormock.h"
#include "network/tests/mocks/networkmanagermock.h"
#include "global/tests/mocks/systeminfomock.h"

#include "update/internal/updateservice.h"

#include "modularity/ioc.h"
#include "global/iapplication.h"

using namespace mu;
using namespace mu::update;

class UpdateServiceTests : public ::testing::Test
{
    Inject<IApplication> application;
public:
    void SetUp() override
    {
        m_service = new UpdateService();

        m_configuration = std::make_shared<UpdateConfigurationMock>();
        m_service->setconfiguration(m_configuration);

        m_networkManagerCreator = std::make_shared<network::NetworkManagerCreatorMock>();
        m_service->setnetworkManagerCreator(m_networkManagerCreator);

        m_networkManager = std::make_shared<network::NetworkManagerMock>();
        ON_CALL(*m_networkManagerCreator, makeNetworkManager())
        .WillByDefault(Return(m_networkManager));

        m_systemInfoMock = std::make_shared<SystemInfoMock>();
        m_service->setsystemInfo(m_systemInfoMock);

        ON_CALL(*m_systemInfoMock, productType())
        .WillByDefault(Return(ISystemInfo::ProductType::Linux));
    }

    void TearDown() override
    {
        delete m_service;
    }

    void makeReleaseInfo() const
    {
        std::string checkForUpdateUrl = "checkForUpdateUrl";
        EXPECT_CALL(*m_configuration, checkForUpdateUrl())
        .WillOnce(Return(checkForUpdateUrl));

        QString releasesNotes = "{"
                                "\"tag_name\": \"v5.0\","
                                "\"assets\": ["
                                "{ \"name\": \"MuseScore.dmg\", \"browser_download_url\": \"blabla\" },"
                                "{ \"name\": \"MuseScore.msi\", \"browser_download_url\": \"blabla\" },"
                                "{ \"name\": \"MuseScore.AppImage\", \"browser_download_url\": \"blabla\" }"
                                "],"
                                "\"assetsNew\": ["
                                "{ \"name\": \"MuseScore-arm.AppImage\", \"browser_download_url\": \"blabla\" },"
                                "{ \"name\": \"MuseScore-aarch64.AppImage\", \"browser_download_url\": \"blabla\" }"
                                "]"
                                "}";

        EXPECT_CALL(*m_networkManager, get(QUrl(QString::fromStdString(checkForUpdateUrl)), _, _))
        .WillOnce(testing::Invoke(
                      [releasesNotes](const QUrl&, network::IncomingDevice* buf, const network::RequestHeaders&) {
            buf->open(network::IncomingDevice::WriteOnly);
            buf->write(releasesNotes.toUtf8());
            buf->close();

            return make_ok();
        }));
    }

    void makePreviousReleasesNotes() const
    {
        std::string previousReleasesNotesUrl = "previousReleasesNotesUrl";
        EXPECT_CALL(*m_configuration, previousReleasesNotesUrl())
        .WillOnce(Return(previousReleasesNotesUrl));

        //! [GIVEN] Previous releases notes. Contains chaotic order of versions
        QString releasesNotes = QString("{"
                                        "\"releases\": ["
                                        "{ \"version\": \"40000.3\", \"notes\": \"blabla3\" },"
                                        "{ \"version\": \"40000.4\", \"notes\": \"blabla4\" },"
                                        "{ \"version\": \"%1\", \"notes\": \"blabla2\" },"
                                        "{ \"version\": \"0.4.1\", \"notes\": \"blabla1\" }"
                                        "]"
                                        "}").arg(application()->fullVersion().toString());

        EXPECT_CALL(*m_networkManager, get(QUrl(QString::fromStdString(previousReleasesNotesUrl)), _, _))
        .WillOnce(testing::Invoke(
                      [releasesNotes](const QUrl&, network::IncomingDevice* buf, const network::RequestHeaders&) {
            buf->open(network::IncomingDevice::WriteOnly);
            buf->write(releasesNotes.toUtf8());
            buf->close();

            return make_ok();
        }));
    }

    UpdateService* m_service = nullptr;
    std::shared_ptr<UpdateConfigurationMock> m_configuration;

    std::shared_ptr<network::NetworkManagerCreatorMock> m_networkManagerCreator;
    std::shared_ptr<network::NetworkManagerMock> m_networkManager;
    std::shared_ptr<SystemInfoMock> m_systemInfoMock;
};

TEST_F(UpdateServiceTests, ParseRelease_Linux_x86_64)
{
    //! [GIVEN] Release info
    makeReleaseInfo();
    makePreviousReleasesNotes();

    //! [GIVEN] System is Linux x86_64
    ON_CALL(*m_systemInfoMock, productType())
    .WillByDefault(Return(ISystemInfo::ProductType::Linux));

    ON_CALL(*m_systemInfoMock, cpuArchitecture())
    .WillByDefault(Return(ISystemInfo::CpuArchitecture::x86_64));

    //! [WHEN] Check for update
    mu::RetVal<ReleaseInfo> retVal = m_service->checkForUpdate();

    //! [THEN] Should return correct release file
    EXPECT_TRUE(retVal.ret);
    EXPECT_EQ(retVal.val.fileName, "MuseScore.AppImage");
}

TEST_F(UpdateServiceTests, ParseRelease_Linux_arm)
{
    //! [GIVEN] Release info
    makeReleaseInfo();
    makePreviousReleasesNotes();

    //! [GIVEN] System is Linux arm
    ON_CALL(*m_systemInfoMock, productType())
    .WillByDefault(Return(ISystemInfo::ProductType::Linux));

    ON_CALL(*m_systemInfoMock, cpuArchitecture())
    .WillByDefault(Return(ISystemInfo::CpuArchitecture::Arm));

    //! [WHEN] Check for update
    mu::RetVal<ReleaseInfo> retVal = m_service->checkForUpdate();

    //! [THEN] Should return correct release file
    EXPECT_TRUE(retVal.ret);
    EXPECT_EQ(retVal.val.fileName, "MuseScore-arm.AppImage");
}

TEST_F(UpdateServiceTests, ParseRelease_Linux_aarch64)
{
    //! [GIVEN] Release info
    makeReleaseInfo();
    makePreviousReleasesNotes();

    //! [GIVEN] System is Linux arm64
    ON_CALL(*m_systemInfoMock, productType())
    .WillByDefault(Return(ISystemInfo::ProductType::Linux));

    ON_CALL(*m_systemInfoMock, cpuArchitecture())
    .WillByDefault(Return(ISystemInfo::CpuArchitecture::Arm64));

    //! [WHEN] Check for update
    mu::RetVal<ReleaseInfo> retVal = m_service->checkForUpdate();

    //! [THEN] Should return correct release file
    EXPECT_TRUE(retVal.ret);
    EXPECT_EQ(retVal.val.fileName, "MuseScore-aarch64.AppImage");
}

TEST_F(UpdateServiceTests, ParseRelease_Linux_Unknown)
{
    //! [GIVEN] Release info
    makeReleaseInfo();
    makePreviousReleasesNotes();

    //! [GIVEN] System is Linux Unknown
    ON_CALL(*m_systemInfoMock, productType())
    .WillByDefault(Return(ISystemInfo::ProductType::Linux));

    ON_CALL(*m_systemInfoMock, cpuArchitecture())
    .WillByDefault(Return(ISystemInfo::CpuArchitecture::Unknown));

    //! [WHEN] Check for update
    mu::RetVal<ReleaseInfo> retVal = m_service->checkForUpdate();

    //! [THEN] Should return correct release file
    EXPECT_TRUE(retVal.ret);
    EXPECT_EQ(retVal.val.fileName, "MuseScore.AppImage");
}

TEST_F(UpdateServiceTests, ParseRelease_Windows)
{
    //! [GIVEN] Release info
    makeReleaseInfo();
    makePreviousReleasesNotes();

    //! [GIVEN] System is Windows, cpuArchitecture isn't important
    ON_CALL(*m_systemInfoMock, productType())
    .WillByDefault(Return(ISystemInfo::ProductType::Windows));

    EXPECT_CALL(*m_systemInfoMock, cpuArchitecture())
    .Times(1);

    //! [WHEN] Check for update
    mu::RetVal<ReleaseInfo> retVal = m_service->checkForUpdate();

    //! [THEN] Should return correct release file
    EXPECT_TRUE(retVal.ret);
    EXPECT_EQ(retVal.val.fileName, "MuseScore.msi");
}

TEST_F(UpdateServiceTests, ParseRelease_MacOS)
{
    //! [GIVEN] Release info
    makeReleaseInfo();
    makePreviousReleasesNotes();

    //! [GIVEN] System is MacOS, cpuArchitecture isn't important
    ON_CALL(*m_systemInfoMock, productType())
    .WillByDefault(Return(ISystemInfo::ProductType::MacOS));

    EXPECT_CALL(*m_systemInfoMock, cpuArchitecture())
    .Times(1);

    //! [WHEN] Check for update
    mu::RetVal<ReleaseInfo> retVal = m_service->checkForUpdate();

    //! [THEN] Should return correct release file
    EXPECT_TRUE(retVal.ret);
    EXPECT_EQ(retVal.val.fileName, "MuseScore.dmg");
}

TEST_F(UpdateServiceTests, CheckForUpdate_ReleasesNotes)
{
    //! [GIVEN] Release info
    makeReleaseInfo();
    makePreviousReleasesNotes();

    //! [THEN] Versions should be in correct order and don't contain current version
    PrevReleasesNotesList expectedReleasesNotes = {
        { "40000.3", "blabla3" },
        { "40000.4", "blabla4" },
    };

    //! [WHEN] Check for update
    mu::RetVal<ReleaseInfo> retVal = m_service->checkForUpdate();

    //! [THEN] Should return correct release file
    EXPECT_TRUE(retVal.ret);
    EXPECT_EQ(retVal.val.previousReleasesNotes, expectedReleasesNotes);
}

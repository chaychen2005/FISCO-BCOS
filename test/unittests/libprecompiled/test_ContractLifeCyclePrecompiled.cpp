/*
 * @CopyRight:
 * FISCO-BCOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FISCO-BCOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>
 * (c) 2016-2018 fisco-dev contributors.
 */

#include "../libstorage/MemoryStorage.h"
#include "libstoragestate/StorageStateFactory.h"
#include <libblockverifier/ExecutiveContextFactory.h>
#include <libdevcrypto/Common.h>
#include <libethcore/ABI.h>
#include <libprecompiled/ContractLifeCyclePrecompiled.h>
#include <libprecompiled/TableFactoryPrecompiled.h>
#include <libstorage/MemoryTable.h>
#include <libstorage/MemoryTableFactoryFactory.h>
#include <libstoragestate/StorageState.h>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace dev;
using namespace dev::blockverifier;
using namespace dev::storage;
using namespace dev::storagestate;
using namespace dev::precompiled;

namespace test_ContractLifeCyclePrecompiled
{
class MockMemoryTableFactory : public dev::storage::MemoryTableFactory
{
public:
    virtual ~MockMemoryTableFactory(){};
};
struct ContractLifeCyclePrecompiledFixture
{
    ContractLifeCyclePrecompiledFixture()
    {
        g_BCOSConfig.setSupportedVersion("2.3.0", V2_3_0);
        contractAddress = Address("0xa4adafef4c989e17675479565b9abb5821d81f2c");

        context = std::make_shared<ExecutiveContext>();
        blockInfo.hash = h256(0);
        blockInfo.number = 0;
        context->setBlockInfo(blockInfo);

        auto storage = std::make_shared<MemoryStorage>();
        auto storageStateFactory = std::make_shared<StorageStateFactory>(h256(0));
        ExecutiveContextFactory factory;
        auto tableFactoryFactory = std::make_shared<MemoryTableFactoryFactory>();
        factory.setStateStorage(storage);
        factory.setStateFactory(storageStateFactory);
        factory.setTableFactoryFactory(tableFactoryFactory);
        factory.initExecutiveContext(blockInfo, h256(0), context);
        memoryTableFactory = context->getMemoryTableFactory();

        tableFactoryPrecompiled = std::make_shared<dev::precompiled::TableFactoryPrecompiled>();
        tableFactoryPrecompiled->setMemoryTableFactory(memoryTableFactory);
        clcPrecompiled = context->getPrecompiled(Address(0x1007));

        // createTable
        std::string tableName("c_" + contractAddress.hex());
        table = memoryTableFactory->createTable(tableName, STORAGE_KEY, STORAGE_VALUE, false);
        if (!table)
        {
            LOG(ERROR) << LOG_BADGE("test_ContractLifeCyclePrecompiled")
                       << LOG_DESC("createAccount failed") << LOG_KV("Account", tableName);
        }
        auto entry = table->newEntry();
        entry->setField(STORAGE_KEY, ACCOUNT_BALANCE);
        entry->setField(STORAGE_VALUE, "0");
        table->insert(ACCOUNT_BALANCE, entry);
        entry = table->newEntry();
        entry->setField(STORAGE_KEY, ACCOUNT_CODE_HASH);
        entry->setField(STORAGE_VALUE, toHex(h256("123456")));
        table->insert(ACCOUNT_CODE_HASH, entry);
        entry = table->newEntry();
        entry->setField(STORAGE_KEY, ACCOUNT_CODE);
        entry->setField(STORAGE_VALUE, "");
        table->insert(ACCOUNT_CODE, entry);
        entry = table->newEntry();
        entry->setField(STORAGE_KEY, ACCOUNT_NONCE);
        entry->setField(STORAGE_VALUE, "0");
        table->insert(ACCOUNT_NONCE, entry);
        entry = table->newEntry();
        entry->setField(STORAGE_KEY, ACCOUNT_ALIVE);
        entry->setField(STORAGE_VALUE, "true");
        table->insert(ACCOUNT_ALIVE, entry);
    }

    ~ContractLifeCyclePrecompiledFixture() {}

    ExecutiveContext::Ptr context;
    TableFactory::Ptr memoryTableFactory;
    Precompiled::Ptr clcPrecompiled;
    dev::precompiled::TableFactoryPrecompiled::Ptr tableFactoryPrecompiled;
    BlockInfo blockInfo;
    Table::Ptr table;
    Address contractAddress;
};

BOOST_FIXTURE_TEST_SUITE(ContractLifeCyclePrecompiled, ContractLifeCyclePrecompiledFixture)

BOOST_AUTO_TEST_CASE(all)
{
    LOG(INFO) << LOG_BADGE("test_ContractLifeCyclePrecompiled") << LOG_DESC("freeze");

    eth::ContractABI abi;
    bytes in;
    bytes out;
    s256 result = 0;

    // CODE_INVALID_NO_AUTHORIZED
    in = abi.abiIn("freeze(address)", contractAddress);
    out = clcPrecompiled->call(context, bytesConstRef(&in));
    abi.abiOut(&out, result);
    BOOST_TEST(result == -51905);

    // freeze success
    auto entry = table->newEntry();
    entry->setField(STORAGE_KEY, ACCOUNT_AUTHORITY);
    entry->setField(STORAGE_VALUE, "0000000000000000000000000000000000000000");
    table->insert(ACCOUNT_AUTHORITY, entry);
    out = clcPrecompiled->call(context, bytesConstRef(&in));
    abi.abiOut(&out, result);
    BOOST_TEST(result == 1);

    // repetitive freeze, CODE_INVALID_CONTRACT_FEOZEN
    out = clcPrecompiled->call(context, bytesConstRef(&in));
    abi.abiOut(&out, result);
    BOOST_TEST(result == -51900);

    // getStatus
    in = abi.abiIn("getStatus(address)", contractAddress);
    out = clcPrecompiled->call(context, bytesConstRef(&in));
    string str;
    abi.abiOut(&out, result, str);
    BOOST_TEST(result == 0);
    BOOST_TEST(str == CONTRACT_STATUS_DESC[2]);

    // unfreeze success
    in = abi.abiIn("unfreeze(address)", contractAddress);
    out = clcPrecompiled->call(context, bytesConstRef(&in));
    abi.abiOut(&out, result);
    BOOST_TEST(result == 1);

    // repetitive unfreeze, CODE_INVALID_CONTRACT_AVAILABLE
    out = clcPrecompiled->call(context, bytesConstRef(&in));
    abi.abiOut(&out, result);
    BOOST_TEST(result == -51901);

    // grantManager success
    in = abi.abiIn("grantManager(address,address)", contractAddress,
        Address("0xa4adafef4c989e17675479565b9abb5821d81f2c"));
    out = clcPrecompiled->call(context, bytesConstRef(&in));
    abi.abiOut(&out, result);
    BOOST_TEST(result == 1);

    // repetitive grantManager success, CODE_INVALID_CONTRACT_REPEAT_AUTHORIZATION
    out = clcPrecompiled->call(context, bytesConstRef(&in));
    abi.abiOut(&out, result);
    BOOST_TEST(result == -51902);

    // listManager(address)
    in = abi.abiIn("listManager(address)", contractAddress);
    out = clcPrecompiled->call(context, bytesConstRef(&in));
    std::vector<Address> addrs;
    abi.abiOut(&out, result, addrs);
    BOOST_TEST(2 == addrs.size());
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace test_ContractLifeCyclePrecompiled

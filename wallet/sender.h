#pragma once

#include "wallet/common.h"
#include "wallet/wallet_db.h"

#include <boost/optional.hpp>
#include "utility/logger.h"

namespace beam::wallet
{
    namespace events
    {
        struct TxRegistrationCompleted
        {
            Uuid m_txId;
        };
        struct TxConfirmationCompleted2 { ConfirmTransaction data; };
        struct TxSenderInvited {};
        struct TxReceiverInvited {};
        struct TxSend {};
        struct TxBill {};
        struct TxConfirmationCompleted { ConfirmTransaction data; };
        struct TxInvitationCompleted { ConfirmInvitation data; };
        struct TxOutputsConfirmed {};
        struct TxFailed
        {
            bool m_notify;
            TxFailed(bool notify = false) : m_notify{ notify } {}
        };
    }

    class Negotiator
    {
    public:
        using Ptr = std::shared_ptr<Negotiator>;

        Negotiator(INegotiatorGateway& gateway
             , beam::IKeyChain::Ptr keychain
             , const TxDescription& txDesc )
            : m_gateway{gateway}
            , m_keychain{keychain}
            , m_txDesc{txDesc}
            , m_fsm{std::ref(*this)}
        {
            assert(keychain);
            m_blindingExcess = ECC::Zero;
        }

        Negotiator(INegotiatorGateway& gateway
            , beam::IKeyChain::Ptr keychain
            , const TxDescription& txDesc
            , InviteReceiver& inviteMsg)
            : Negotiator{ gateway , keychain, txDesc}
        {
            assert(keychain);
            m_offset = inviteMsg.m_offset;
            m_publicPeerExcess = inviteMsg.m_publicSenderExcess;
            setPublicPeerNonce(inviteMsg.m_publicSenderNonce);
            m_transaction = std::make_shared<Transaction>();
            m_transaction->m_Offset = ECC::Zero;
            m_transaction->m_vInputs = move(inviteMsg.m_inputs);
            m_transaction->m_vOutputs = move(inviteMsg.m_outputs);
        }
        void start()
        {
            m_fsm.start();
        }

        void stop()
        {
            m_fsm.stop();
        }

        template<typename Event>
        bool process_event(const Event& event)
        {
            auto res = m_fsm.process_event(event) == msm::back::HANDLED_TRUE;
            return res;
        }

        template<class Archive>
        void serialize(Archive & ar, const unsigned int)
        {
            m_fsm.serialize(ar, 0);
        }

        // for test only
        const int* current_state() const
        {
            return m_fsm.current_state();
        }

        struct FSMDefinition : public msmf::state_machine_def<FSMDefinition>
        {
            // states
            struct TxAllOk : public msmf::state<>
            {

            };

            struct TxInitial : public msmf::state<>
            {
                template <class Event, class Fsm>
                void on_entry(Event const&, Fsm&)
                {
                    LOG_VERBOSE() << "TxInitial state";
                }
            };
            struct TxTerminal : public msmf::terminate_state<>
            {
                template <class Event, class Fsm>
                void on_entry(Event const&, Fsm& fsm)
                {
                    LOG_VERBOSE() << "TxTerminal state";
                    fsm.m_parent.m_gateway.on_tx_completed(fsm.m_parent.m_txDesc);
                }
            };
            struct TxReceiverInvitation : public msmf::state<>
            {
                template <class Event, class Fsm>
                void on_entry(Event const&, Fsm&)
                {
                    LOG_VERBOSE() << "TxReceiverInvitation state";
                }
            };
            struct TxSenderInvitation : public msmf::state<>
            {
                template <class Event, class Fsm>
                void on_entry(Event const&, Fsm&)
                {
                    LOG_VERBOSE() << "TxSenderInvitation state";
                }
            };
            struct TxBillConfirmation : public msmf::state<>
            {
                template <class Event, class Fsm>
                void on_entry(Event const&, Fsm&)
                {
                    LOG_VERBOSE() << "TxBillConfirmation state";
                }
            };
            struct TxSendingConfirmation : public msmf::state<>
            {
                template <class Event, class Fsm>
                void on_entry(Event const&, Fsm&)
                {
                    LOG_VERBOSE() << "TxSendingConfirmation state";
                }
            };
            struct TxSenderConfirmation : public msmf::state<>
            {
                template <class Event, class Fsm>
                void on_entry(Event const&, Fsm&)
                {
                    LOG_VERBOSE() << "TxSenderConfirmation state";
                }
            };
            struct TxReceiverConfirmation : public msmf::state<>
            {
                template <class Event, class Fsm>
                void on_entry(Event const&, Fsm&)
                {
                    LOG_VERBOSE() << "TxReceiverConfirmation state";
                }
            };
            struct TxRegistration : public msmf::state<>
            {
                template <class Event, class Fsm>
                void on_entry(Event const&, Fsm&)
                {
                    LOG_VERBOSE() << "TxRegistration state";
                }
            };
            struct TxOutputsConfirmation : public msmf::state<>
            {
                template <class Event, class Fsm>
                void on_entry(Event const&, Fsm&)
                {
                    LOG_VERBOSE() << "TxOutputsConfirmation state";
                }
            };

            FSMDefinition(Negotiator& parent)
                : m_parent{parent}
            {
                update_tx_description(TxDescription::Pending);
            }

            // transition actions
            bool isValidSignature(const events::TxInvitationCompleted&);
            bool isValidSignature(const events::TxConfirmationCompleted&);
            void rollbackTx(const events::TxFailed& );
            void completeTx();
            void rollbackTx();

            void confirmSenderInvitation(const events::TxSenderInvited&);
            void confirmReceiverInvitation(const events::TxReceiverInvited&);
            void inviteReceiver(const events::TxSend&);
            void inviteSender(const events::TxBill&);
            void registerTx(const events::TxConfirmationCompleted&);
            void confirmReceiver(const events::TxInvitationCompleted&);
            void confirmSender(const events::TxInvitationCompleted&);
            void confirmOutputs(const events::TxRegistrationCompleted&);
            void confirmOutputs(const events::TxConfirmationCompleted&);
            void completeTx(const events::TxOutputsConfirmed&);
            void completeTx(const events::TxRegistrationCompleted&);
            void completeTx(const events::TxConfirmationCompleted&);

            Amount get_total() const;

            void update_tx_description(TxDescription::Status s);

            using do_serialize = int;
            typedef int no_message_queue;

            using initial_state = mpl::vector<TxInitial, TxAllOk>;
            using d = FSMDefinition;

            struct transition_table : mpl::vector<
                //   Start                      Event                             Next                  Action                     Guard
                a_row< TxInitial              , events::TxSenderInvited         , TxBillConfirmation    , &d::confirmSenderInvitation    >,
                a_row< TxInitial              , events::TxReceiverInvited       , TxSendingConfirmation , &d::confirmReceiverInvitation  >,
                a_row< TxInitial              , events::TxSend                  , TxReceiverInvitation  , &d::inviteReceiver             >,
                a_row< TxInitial              , events::TxBill                  , TxSenderInvitation    , &d::inviteSender               >,

                a_row< TxBillConfirmation     , events::TxConfirmationCompleted , TxRegistration        , &d::registerTx                 >,
                a_row< TxSendingConfirmation  , events::TxConfirmationCompleted , TxRegistration        , &d::registerTx                 >,
                a_row< TxReceiverInvitation   , events::TxInvitationCompleted   , TxReceiverConfirmation, &d::confirmReceiver            >,
                a_row< TxSenderInvitation     , events::TxInvitationCompleted   , TxSenderConfirmation  , &d::confirmSender              >,

                a_row< TxRegistration         , events::TxRegistrationCompleted , TxTerminal            , &d::completeTx                 >,
                a_row< TxReceiverConfirmation , events::TxConfirmationCompleted , TxTerminal            , &d::completeTx                 >,
                a_row< TxSenderConfirmation   , events::TxConfirmationCompleted , TxTerminal            , &d::completeTx                 >,

                //a_row< TxRegistration         , events::TxRegistrationCompleted , TxOutputsConfirmation , &d::confirmOutputs             >,
                //a_row< TxReceiverConfirmation , events::TxConfirmationCompleted , TxOutputsConfirmation , &d::confirmOutputs             >,
                //a_row< TxSenderConfirmation   , events::TxConfirmationCompleted , TxOutputsConfirmation , &d::confirmOutputs             >,

                //a_row< TxOutputsConfirmation  , events::TxOutputsConfirmed      , TxTerminal            , &d::completeTx                 >,

                a_row< TxAllOk                , events::TxFailed                , TxTerminal            , &d::rollbackTx                 >
            > {};


            template <class FSM, class Event>
            void no_transition(Event const& e, FSM& fsm, int state)
            {
                LOG_DEBUG() << "[Sender] no transition from state " << state
                            << " on event " << typeid(e).name();
            }

            template <class FSM, class Event>
            void exception_caught(Event const&, FSM& fsm, std::exception& ex)
            {
                LOG_ERROR() << ex.what();
                fsm.process_event(events::TxFailed());
            }

            template<typename Archive>
            void serialize(Archive & ar, const unsigned int)
            {
              //  ar  & m_blindingExcess
              //      & m_kernel;
            }
            Negotiator& m_parent;
        };

    private:
        void createKernel(Amount fee, Height minHeight);
        Input::Ptr createInput(const Coin& utxo);
        Output::Ptr createOutput(Amount amount, Height height);
        void setPublicPeerNonce(const ECC::Point& publicPeerNonce);
        ECC::Scalar createSignature();
        void createSignature2(ECC::Scalar& partialSignature, ECC::Point& publicNonce);
        ECC::Point getPublicExcess();
        ECC::Point getPublicNonce();
        bool isValidSignature(const ECC::Scalar& peerSignature);
        bool isValidSignature(const ECC::Scalar& peerSignature, const ECC::Point& publicPeerNonce, const ECC::Point& publicPeerExcess);

    private:
        using Fsm = msm::back::state_machine<FSMDefinition>;
        friend Fsm;
        
        INegotiatorGateway& m_gateway;
        beam::IKeyChain::Ptr m_keychain;

        TxDescription m_txDesc;

        ECC::Scalar::Native m_blindingExcess;
        ECC::Scalar::Native m_offset;
        ECC::Scalar::Native m_peerSignature;
        ECC::Point::Native m_publicPeerExcess;
        ECC::Point::Native m_publicPeerNonce;
        Transaction::Ptr m_transaction;
        TxKernel::Ptr m_kernel;

        Fsm m_fsm;
    };
}

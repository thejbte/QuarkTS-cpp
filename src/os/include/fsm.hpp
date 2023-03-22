#pragma once

#include "types.hpp"
#include "timer.hpp"
#include "queue.hpp"

namespace qOS {

    class stateMachine;
    
    namespace fsm {
        class state;

        typedef enum {
            SIGNAL_START = 0xFFFFFFFFuL,
            SIGNAL_EXIT = 0xFFFFFFFEuL,
            SIGNAL_ENTRY = 0xFFFFFFFDuL,
            SIGNAL_NONE = 0xFFFFFFFCuL,
            MAX_SIGNAL = 0xFFFFFFFBuL,
            MIN_SIGNAL = 0x0uL,
            TM_MAX = 0xFFFFFFFBuL,
            TM_MIN = TM_MAX - ( Q_FSM_MAX_TIMEOUTS - 1 ),
        } signalID;

        constexpr signalID SIGNAL_TIMEOUT( index_t iTm ) 
        {
            return static_cast<signalID>( signalID::TM_MAX - static_cast<fsm::signalID>( Q_FSM_MAX_TIMEOUTS - 1 ) + static_cast<fsm::signalID>( iTm ) );
        }

        typedef struct {
            signalID id;
            void *data{ nullptr };
        } signal_t;

        typedef enum {
            BEFORE_ANY = -32767,
            ABSENT = -32766,
            FAILURE = -32765,
            SUCCESS = -32764,
            SIGNAL_HANDLED = -32763,
        } status;

        typedef enum {
            NO_HISTORY = 0,
            SHALLOW_HISTORY,
            DEEP_HISTORY,
        } historyMode;

        class _Handler {
            protected:
                state *StartState{ nullptr };
                state *NextState{ nullptr };
                stateMachine* Machine{ nullptr };
                state *State{ nullptr };
                historyMode TransitionHistory{ NO_HISTORY };
                status Status{ SUCCESS };
                signalID Signal;
                _Handler( _Handler const& ) = delete;      /* not copyable*/
                void operator=( _Handler const& ) = delete;  /* not assignable*/
            public:
                _Handler() = default;
                void *SignalData{ nullptr };
                void *Data{ nullptr };
                void *StateData{ nullptr };
                void nextState( state *s )
                {
                    NextState = s;
                }
                void nextState( state *s, historyMode m )
                {
                    NextState = s;
                    TransitionHistory = m;
                }
                bool timeoutSet( index_t i, time_t t );
                bool timeoutStop( index_t i );
                state* thisState( void ) const { return State; }
                stateMachine* thisMachine( void ) const { return Machine; }
                signalID signal( void ) const { return Signal; }
                status status( void ) const { return Status; }
            friend class qOS::fsm::state;
        };
        typedef _Handler& handler_t;

        typedef status (*stateCallback_t)( handler_t h );
        typedef void (*surroundingCallback_t)( handler_t h );
        typedef bool (*signalAction_t)( handler_t h );

        typedef uint32_t timeoutSpecOption_t;

        typedef struct {
            time_t xTimeout;
            timeoutSpecOption_t options;
        } timeoutStateDefinition_t;

        typedef struct {
            signalID xSignal;
            signalAction_t guard{ nullptr };
            state *nextState{ nullptr };
            historyMode historyMode{ NO_HISTORY };
            void *signalData;
        } transition_t;

        class state {
            protected:
                state *parent{ nullptr };
                state *lastRunningChild{ nullptr };
                state *initState{ nullptr };
                stateCallback_t sCallback{ nullptr};
                timeoutStateDefinition_t *tdef{ nullptr };
                transition_t *tTable{ nullptr };
                void *sData{ nullptr };
                size_t tEntries{ 0u };
                size_t nTm{ 0u };
                void sweepTransitionTable( _Handler &h );
                state( state const& ) = delete;      /* not copyable*/
                void operator=( state const& ) = delete;  /* not assignable*/
            public:
                state() = default;
                bool setTransitions( transition_t *table, size_t n );
                bool setTimeouts( timeoutStateDefinition_t *tdef, size_t n );
                state* getInit( void ) const;
                state* getLastState( void ) const;
                state* getParent( void ) const;
                void* getData( void ) const;
                transition_t* getTransitionTable( void ) const;
                void setCallback( stateCallback_t sFcn );
            friend class qOS::stateMachine;
        };

        typedef struct {
            uint32_t isPeriodic{ 0u};
            timer timeout[ Q_FSM_MAX_TIMEOUTS ];
        } timeoutSpec_t;

        typedef enum {
            PS_SIGNAL_NOT_FOUND,
            PS_SUBSCRIBER_NOT_FOUND,
            PS_SUBSCRIBER_FOUND,
            PS_SIGNAL_SLOTS_FULL,
            PS_SUBSCRIBER_SLOTS_FULL
        } psReqStatus;

        typedef struct {
            psReqStatus status;
            size_t sig_slot, sub_slot;
        } psIndex_t;

    }

    class stateMachine : protected fsm::_Handler {
        protected:
            fsm::state *current{ nullptr };
            fsm::state *next{ nullptr };
            fsm::state *source{ nullptr };
            queue *sQueue{ nullptr };
            fsm::timeoutSpec_t *timeSpec{ nullptr };
            fsm::surroundingCallback_t surrounding{ nullptr };
            fsm::state top;
            //fsm::_Handler handler;
            fsm::signal_t signalNot;
            void *owner{ nullptr };
            void unsubscribeAll( void );
            bool internalSignalSend( fsm::signalID sig, void *sData, bool isUrgent );
            void timeoutCheckSignals( void );
            void timeoutPerformSpecifiedActions( fsm::state *s, fsm::signalID sig );
            fsm::psIndex_t getSubscriptionStatus( fsm::signalID s );
            void transition( fsm::state *target, fsm::historyMode mHistory );
            uint8_t levelsToLCA( fsm::state *target );
            void exitUpToLCA( uint8_t lca );
            void prepareHandler( fsm::signal_t sig, fsm::state *s );
            fsm::status invokeStateCallback( fsm::state *s );
            fsm::state* stateOnExit( fsm::state *s );
            void stateOnEntry( fsm::state *s );
            fsm::state* stateOnStart( fsm::state *s );
            fsm::status stateOnSignal( fsm::state *s, fsm::signal_t sig );
            void tracePathAndRetraceEntry( fsm::state **trace );
            void traceOnStart( fsm::state **entryPath );
            fsm::signal_t checkForSignals( fsm::signal_t sig );
            stateMachine( stateMachine const& ) = delete;      /* not copyable*/
            void operator=( stateMachine const& ) = delete;  /* not assignable*/
        public:
            void *mData{ nullptr };
            stateMachine() = default;
            bool setup( fsm::stateCallback_t topFcn, fsm::state *init, fsm::surroundingCallback_t sFcn, void* pData );
            bool stateSubscribe( fsm::state *s, fsm::state *parent, fsm::stateCallback_t sFcn, fsm::state *init, void *pData );
            bool installSignalQueue( queue *q );
            bool sendSignal( fsm::signalID sig, void *sData, bool isUrgent );
            bool sendSignalToSubscribers( fsm::signalID sig, void *sData, bool isUrgent );
            bool installTimeoutSpec( fsm::timeoutSpec_t *ts );
            bool timeoutSet( index_t xTimeout, time_t t );
            bool timeoutStop( index_t xTimeout );
            const fsm::state* getTop( void ) const;
            fsm::state* getCurrent( void ) const;
            queue* getQueue( void ) const;
            fsm::timeoutSpec_t* getTimeSpec( void ) const;
            void* getData( void ) const;
            void setSurrounding( fsm::surroundingCallback_t sFcn );
            bool subscribeToSignal( fsm::signalID s );
            bool unsubscribeFromSignal( fsm::signalID s );
            bool run( fsm::signal_t sig );

            static const fsm::timeoutSpecOption_t TSOPT_SET_ENTRY;
            static const fsm::timeoutSpecOption_t TSOPT_RST_ENTRY;
            static const fsm::timeoutSpecOption_t TSOPT_SET_EXIT;
            static const fsm::timeoutSpecOption_t TSOPT_RST_EXIT;
            static const fsm::timeoutSpecOption_t TSOPT_KEEP_IF_SET;
            static const fsm::timeoutSpecOption_t TSOPT_PERIODIC;
            static constexpr fsm::timeoutSpecOption_t TSOPT_INDEX( index_t i );
        friend class core;
    };

}

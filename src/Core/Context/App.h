#pragma once

namespace core
{
    class AppContext
    {
    public:
        static AppContext& getInstance()
        {
            static AppContext instance;
            return instance;
        }

        AppContext(const AppContext&) = delete;
        AppContext& operator=(const AppContext&) = delete;

        void initialize();
        void cleanup();

    private:
        void initAsyncLogger();

    private:
        AppContext() = default;
    };
}

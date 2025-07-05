#pragma once

namespace core
{
    class AppContext
    {
    public:
        static AppContext& getInstance()
        {
            static AppContext s_instance;
            return s_instance;
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

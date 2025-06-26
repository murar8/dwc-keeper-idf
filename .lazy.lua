return {
	{
		"neovim/nvim-lspconfig",
		optional = true,
		opts = {
			servers = {
				clangd = {
					root_dir = function()
						-- leave empty to stop nvim from cd'ing into ~/ due to global .clangd file
					end,
					cmd = {
						"/home/lmurarotto/.espressif/tools/esp-clang/esp-18.1.2_20240912/esp-clang/bin/clangd",
						"--background-index",
						"--query-driver=**",
					},
				},
			},
		},
	},
}

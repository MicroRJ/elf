function Link(link)
	local path, suffix = link.target:match("^(.-)%.md(.*)$")
	if path then
		if path:match("/README$") then path = path:gsub("/README$", "/index") end
		link.target = path .. ".html" .. suffix
	end
	return link
end

function Pandoc(document)
	if not document.meta.title then
		for _, block in ipairs(document.blocks) do
			if block.t == "Header" and block.level == 1 then
				document.meta.title = pandoc.MetaInlines(block.content)
				break
			end
		end
	end
	return document
end
